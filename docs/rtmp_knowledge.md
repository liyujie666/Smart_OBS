
### 6.2 事件循环模型

SDK采用单IO线程事件循环：

```cpp
// 伪代码：IO线程主循环
while (running_) {
    // 1. 准备pollfd
    WSAPOLLFD fds[1];
    fds[0].fd = socket_;
    fds[0].events = POLLIN;  // 总是监听可读（服务器消息）
    
    if (hasDataToSend()) {
        fds[0].events |= POLLOUT;  // 有数据待发送时监听可写
    }
    
    // 2. 等待事件（带超时）
    int ret = WSAPoll(fds, 1, 100);  // 100ms超时
    
    // 3. 处理可读事件
    if (fds[0].revents & POLLIN) {
        readFromSocket();  // 读取ACK、onStatus等
    }
    
    // 4. 处理可写事件
    if (fds[0].revents & POLLOUT) {
        flushPendingData();  // 发送缓冲区的数据
    }
    
    // 5. 从队列取新的音视频帧
    processMediaQueue();
    
    // 6. 定时任务（统计、Ping等）
    checkTimers();
}
```

对应代码：`publisher_impl.cpp` 的 IO 线程。

### 6.3 Partial Write 处理

TCP send 可能只发送部分数据，必须处理：

```cpp
bool Session::flush(size_t& written) {
    written = 0;
    while (out_.remaining() > 0) {
        int sent = send(sockfd_, out_.readPtr(), out_.remaining(), 0);
        
        if (sent > 0) {
            out_.skip(sent);  // 移动读指针
            written += sent;
            totalBytesSent_ += sent;
        } else if (sent == 0) {
            // 连接关闭
            return false;
        } else {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                break;  // 缓冲区满，等待下次POLLOUT
            } else {
                return false;  // 其他错误
            }
        }
    }
    return true;
}
```

---

## 7. 时间戳与音视频同步

### 7.1 时间戳基准

#### 三种时间戳

1. **DTS (Decode Timestamp)**: 解码时间戳
2. **PTS (Presentation Timestamp)**: 显示时间戳
3. **RTMP Timestamp**: RTMP消息时间戳（毫秒）

#### 关系

- 无B帧场景：`PTS = DTS`
- RTMP Timestamp = DTS（视频）或采样时间（音频）
- CompositionTime = PTS - DTS

### 7.2 SDK中的时间戳处理

#### 输入：微秒时基

```cpp
struct MediaSample {
    int64_t dtsUs;  // 微秒
    int64_t ptsUs;  // 微秒
};
```

编码器产生的时间戳基准通常是相对的（从0开始或从某个起始时间）。

#### 转换：微秒 -> RTMP毫秒

```cpp
uint32_t rtmpTimestamp = static_cast<uint32_t>(sample.dtsUs / 1000);
int32_t compositionTime = static_cast<int32_t>((sample.ptsUs - sample.dtsUs) / 1000);
```

#### 时间戳回绕

RTMP时间戳是32位毫秒，最大值约49.7天。超过后会回绕到0。
Extended Timestamp机制解决了这个问题，但实际直播很少超过4.6小时（0xFFFFFF毫秒）。

### 7.3 音视频同步

#### 首帧对齐

第一个音频帧和第一个视频帧的时间戳应该接近：

```cpp
// 建议：以第一个视频关键帧为基准
if (firstVideoTimestamp == 0) {
    firstVideoTimestamp = videoSample.dtsUs;
}
if (firstAudioTimestamp == 0) {
    firstAudioTimestamp = audioSample.dtsUs;
}

// 归一化到0开始
uint32_t videoRtmpTs = (videoSample.dtsUs - firstVideoTimestamp) / 1000;
uint32_t audioRtmpTs = (audioSample.dtsUs - firstAudioTimestamp) / 1000;
```

#### 音视频交错发送

不要先发完所有视频再发音频，应该按时间戳交错：

```cpp
while (hasData) {
    if (nextVideoTs < nextAudioTs) {
        sendVideo();
    } else {
        sendAudio();
    }
}
```

---

## 8. 状态机与会话管理

### 8.1 Session状态机

SDK使用状态机管理连接生命周期：

```cpp
enum class SessionState {
    Idle,              // 初始状态
    Resolving,         // DNS解析（如果需要）
    Connecting,        // TCP连接中
    Handshaking,       // RTMP握手
    AppConnecting,     // 发送connect命令
    StreamCreating,    // 发送createStream命令
    Publishing,        // 发送publish命令
    Streaming,         // 正常推流
    Backoff,           // 断线后退避等待
    Reconnecting,      // 重连中
    Stopped,           // 用户主动停止
    Error              // 错误状态
};
```

#### 状态转换

```
Idle 
  -> Connecting 
  -> Handshaking 
  -> AppConnecting 
  -> StreamCreating 
  -> Publishing 
  -> Streaming
  
Streaming -> Error -> Backoff -> Reconnecting -> Connecting ...
```

对应代码：`session.h` 中的 `SessionState` 枚举。

### 8.2 状态驱动的逻辑

```cpp
bool Session::step(int timeoutMs) {
    switch (state_) {
    case SessionState::Connecting:
        // 检查连接是否完成
        if (isConnected()) {
            handshake_.start(out_);
            setState(SessionState::Handshaking);
        }
        break;
        
    case SessionState::Handshaking:
        // 驱动握手
        if (handshake_.done()) {
            sendConnect();
            setState(SessionState::AppConnecting);
        }
        break;
        
    case SessionState::AppConnecting:
        // 等待 _result(connect)
        break;
        
    case SessionState::Streaming:
        // 正常收发数据
        break;
    }
    
    // 统一的读写处理
    pumpRead();
    flush();
    return state_ != SessionState::Error;
}
```

### 8.3 超时检测

每个状态都应该有超时保护：

```cpp
if (state_ == SessionState::AppConnecting) {
    auto elapsed = now - connectStartedAt_;
    if (elapsed > config_.connectTimeoutMs) {
        setState(SessionState::Error, "connect timeout");
    }
}
```

---

## 9. 背压控制与流控

### 9.1 为什么需要背压控制？

问题场景：
- 编码器产生 5Mbps 的码流
- 网络只能传输 3Mbps
- 发送队列不断积压，延迟越来越高

**解决方案**：背压控制 + GOP级丢帧 + 动态码率调整。

### 9.2 有界队列

使用有界队列限制缓冲深度：

```cpp
class BoundedQueue {
    std::deque<MediaSample> queue_;
    size_t maxSize_ = 100;
    
    bool push(const MediaSample& sample) {
        if (queue_.size() >= maxSize_) {
            // 队列满，触发背压策略
            return false;
        }
        queue_.push_back(sample);
        return true;
    }
};
```

对应代码：`bounded_queue.h`。

### 9.3 队列延迟度量

用媒体时长而非包数量：

```cpp
int queueDelayMs() {
    if (queue_.empty()) return 0;
    int64_t headDts = queue_.front().dtsUs;
    int64_t tailDts = queue_.back().dtsUs;
    return (tailDts - headDts) / 1000;
}
```

### 9.4 三级背压策略

| 状态 | 触发条件 | 动作 |
|------|----------|------|
| Normal | queueDelay < 800ms | 正常发送 |
| Congested | queueDelay > 800ms | 降低码率（回调onRequestBitrate） |
| DropGop | queueDelay > 1200ms | 丢弃最旧GOP + 请求IDR（回调onRequestKeyframe） |

### 9.5 GOP感知丢帧

```cpp
void dropOldestGop() {
    // 1. 找到第一个关键帧位置
    auto it = std::find_if(queue_.begin(), queue_.end(), 
        [](const MediaSample& s) { return s.type == SampleType::VideoKey; });
    
    if (it == queue_.end()) return;
    
    // 2. 找到下一个关键帧位置
    auto nextKey = std::find_if(it + 1, queue_.end(),
        [](const MediaSample& s) { return s.type == SampleType::VideoKey; });
    
    if (nextKey == queue_.end()) return;
    
    // 3. 删除第一个GOP的所有非关键帧（保留关键帧本身）
    queue_.erase(it + 1, nextKey);
    
    // 4. 请求编码器生成新的IDR
    onRequestKeyframe_();
}
```

### 9.6 网络监控指标

```cpp
struct RtmpStats {
    int64_t bytesSent;       // send()成功的字节数
    int64_t bytesAcked;      // 服务器Acknowledgement确认的字节数
    int64_t bytesInflight;   // bytesSent - bytesAcked（真实积压）
    int sendThroughputBps;   // 滑动窗口平均发送速率
    int rttMs;               // Ping/Pong测量的RTT
    int videoQueueDelayMs;   // 视频队列延迟
};
```

**拥塞判定**：
- `bytesInflight` 持续增长 -> 发送速度 > 网络速度
- `rttMs` 增大 -> 网络拥塞
- `queueDelayMs` 增大 -> 生产速度 > 发送速度

对应代码：`stats.h` 和 `metrics.cpp`。

---

## 10. 错误处理与重连策略

### 10.1 断线检测

#### TCP层断线
- `send()` 返回 `ECONNRESET` 或 `EPIPE`
- `recv()` 返回 0（对端关闭）

#### RTMP层断线
- 长时间未收到服务器ACK（如30秒）
- `onStatus` 收到错误码（如 `NetStream.Publish.BadName`）

### 10.2 指数退避重连

```cpp
int backoffMs = reconnectBaseMs * (1 << retryCount);
backoffMs = std::min(backoffMs, reconnectMaxMs);

// 添加随机抖动，避免多个客户端同时重连
int jitter = rand() % (backoffMs / 4);
int finalBackoff = backoffMs + jitter;

std::this_thread::sleep_for(std::chrono::milliseconds(finalBackoff));
```

**退避示例**（base=1000ms, max=30000ms）：
- 第1次：1s + 抖动
- 第2次：2s + 抖动
- 第3次：4s + 抖动
- 第4次：8s + 抖动
- 第5次：16s + 抖动
- 第6次：30s（达到上限）

### 10.3 重连恢复流程

```
断线检测
  -> 进入 Backoff 状态
  -> 等待退避时间
  -> 进入 Reconnecting 状态
  -> 重新 TCP 连接
  -> 重新握手
  -> 重新 connect/createStream/publish
  -> 重发 onMetaData
  -> 重发 AAC Sequence Header
  -> 重发 AVC Sequence Header
  -> 请求 IDR 帧（onRequestKeyframe回调）
  -> 从新关键帧恢复正常推流
```

#### 时间戳处理

重连后时间戳归零：

```cpp
void onReconnected() {
    firstVideoTimestamp_ = 0;
    firstAudioTimestamp_ = 0;
    // 后续帧会重新建立基准
}
```

#### 清理旧队列

```cpp
void onReconnecting() {
    // 丢弃旧的积压数据
    mediaQueue_.clear();
    
    // 保留配置帧（SPS/PPS/ASC）会在重连后重发
}
```

### 10.4 失败上限

避免无限重连：

```cpp
const int kMaxRetries = 10;
if (retryCount_ > kMaxRetries) {
    setState(SessionState::Error, "max retries exceeded");
    stop();
}
```

---

## 11. 总结：从协议到SDK的完整映射

### 11.1 数据流转全景

```
编码器（你的项目）
  |
  | AVPacket (H.264 Annex B, AAC ADTS)
  |
  v
适配层 (ffmpeg_bridge)
  | 转换格式：Annex B -> AVCC, ADTS -> Raw AAC
  | 提取配置：SPS/PPS -> VideoParams, ASC -> AudioParams
  v
SDK Public API (Publisher)
  | pushVideo(MediaSample), pushAudio(MediaSample)
  v
有界队列 (BoundedQueue)
  | 背压控制，GOP感知丢帧
  v
FLV封装 (flv_tag)
  | 构建 AVC/AAC Tag payload
  v
RTMP Message
  | typeId=8/9, payload=FLV Tag
  v
Chunk编码 (ChunkWriter)
  | 分块，头部压缩（fmt 0/1/2/3）
  v
TCP发送缓冲 (ByteBuffer)
  | 非阻塞 send，Partial Write处理
  v
网络传输 (ITransport)
  | WSAPoll 事件循环
  v
RTMP服务器
```

### 11.2 关键代码映射

| 功能模块 | 文件 | 核心逻辑 |
|---------|------|---------|
| 握手 | handshake.cpp | C0/C1/C2三次握手，状态机驱动 |
| AMF0编码 | amf0.cpp | connect/publish命令序列化 |
| Chunk编码 | chunk_writer.cpp | fmt选择，头部压缩，分块 |
| Chunk解码 | chunk_reader.cpp | 重组消息，处理ExtendedTimestamp |
| FLV封装 | flv_tag.cpp | AVC/AAC Sequence Header，NALU/Raw封装 |
| 会话管理 | session.cpp | 状态机，命令流程，ACK处理 |
| 发布门面 | publisher_impl.cpp | IO线程，队列管理，回调触发 |
| 背压控制 | bounded_queue.cpp | 队列延迟度量，GOP丢弃 |
| 网络传输 | tcp_transport_select.cpp | 非阻塞socket，WSAPoll事件循环 |

### 11.3 学习建议

1. **先读协议规范**：理解RTMP Chunk、Message、AMF0的结构
2. **抓包分析**：用Wireshark抓取OBS推流到SRS的包，对照协议解析
3. **调试SDK代码**：在关键点打断点，观察数据转换过程
4. **模拟弱网**：用clumsy限速，观察背压和重连机制
5. **对比FFmpeg**：研究librtmp的实现差异

---

## 12. 参考资料

- **RTMP规范**: Adobe RTMP Specification 1.0
- **FLV格式**: Adobe FLV File Format Specification
- **H.264标准**: ITU-T H.264 / ISO/IEC 14496-10
- **AAC标准**: ISO/IEC 14496-3
- **AMF规范**: Adobe AMF0/AMF3 Specification

---

**文档完成！** 这份文档覆盖了从RTMP协议底层到SDK实现的全部核心知识，配合你的代码阅读，可以完全掌握RTMP推流的技术细节。
