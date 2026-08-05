# 自研 RTMP SDK —— 面试准备文档

## 一、架构总览

```
┌──────────────────────────────────────────────────────────────────────┐
│  Publisher (Facade / Pimpl 模式)       │
│  对外暴露：start / stop / pushVideo / pushAudio / setCallback        │
└────────────┬─────────────────────────────────────────────────────────┘
         │ unique_ptr<PublisherImpl>
     ▼
┌──────────────────────────────────────────────────────────────────────┐
│  PublisherImpl (IO 调度中心)      │
│  拥有：BoundedQueue×2(音频/视频)、Metrics、ioThread_        │
│  驱动：ioLoop() → connect / prime / backpressure / drain / reconnect │
└────────────┬─────────────────────────────────────────────────────────┘
             │ unique_ptr<Session>
       ▼
┌──────────────────────────────────────────────────────────────────────┐
│  Session (协议状态机)    │
│  拥有：Handshake、ChunkWriter、ChunkReader、ITransport        │
│  状态：Handshaking → Connecting → Creating → Publishing → Ready │
│  驱动：step() → pumpRead / flush / onMessage│
└────────────┬─────────────────────────────────────────────────────────┘
      │ unique_ptr<ITransport>
       ▼
┌──────────────────────────────────────────────────────────────────────┐
│  TcpTransport (非阻塞 TCP)              │
│  WSAPoll(Win) / poll(Linux)，TCP_NODELAY     │
│  接口：connect / send / recv / wait / close │
└──────────────────────────────────────────────────────────────────────┘
```

### 数据流
```
生产者线程 → pushVideo/pushAudio → BoundedQueue(mutex+condvar)
IO线程 → drainQueues() → FLV Tag封装 → session_->sendMediaMessage()
       → ChunkWriter::write() → ByteBuffer(out_) → flush() → transport_->send()
```

---

## 二、Handshake 实现

### 状态机
```
Start ──start()──▶ WaitS0S1 ──onData()──▶ WaitS2 ──onData()──▶ Done
          │
      ▼ (S0 != 0x03)
    Failed
```

### 具体步骤
| 阶段 | 操作 |
|------|------|
| C0+C1 发送 | 写入 `0x03`(版本3) + 4字节 time + 4字节 zero + 1528字节随机数 |
| 等待 S0+S1 | 验证 S0==0x03，保存 S1，然后回传 S1 作为 C2 |
| 等待 S2 | 读取 1536 字节，不验证内容（兼容性简化），进入 Done |

### 设计要点
- **增量解析**：`onData()` 可分多次调用，数据不足返回 true 等待更多数据
- **Simple Handshake**：不实现 HMAC-SHA256 complex handshake（兼容 SRS/nginx-rtmp/ZLMediaKit）

---

## 三、Chunk 拆组包

### ChunkWriter（发送端 —— 差分压缩）

**fmt 选择逻辑**：
```cpp
PrevState& prev = prev_[msg.csid];  // 每个 csid 维护前一包状态

if (!prev.valid || msg.streamId != prev.streamId)
    fmt = 0;  // 完整头 (11字节)
else {
    delta = msg.timestamp - prev.timestamp;
    if (msg.typeId == prev.typeId && msg.payload.size() == prev.length)
   fmt = (delta == prev.timestampDelta) ? 3 : 2;  // 无头 / 仅delta
    else
    fmt = 1;  // 省略streamId (7字节)
}
```

| fmt | 头大小 | 包含字段 |
|-----|--------|----------|
| 0 | 11字节 | timestamp + length + typeId + streamId(LE) |
| 1 | 7字节 | delta + length + typeId（省略streamId） |
| 2 | 3字节 | 仅 delta |
| 3 | 0字节 | 全部继承上一包 |

**大消息分片**：超过 `chunkSize_`（默认4096）的消息自动分片，续包用 fmt=3 头。

**Extended Timestamp**：timestamp ≥ 0xFFFFFF 时，头中写 0xFFFFFF，实际值追加在头后面（4字节）。

### ChunkReader（接收端 —— 流式解析）

**Basic Header 解析**（csid 编码）：
```
csid ∈ [2, 63]   → 1字节: fmt<<6 | csid
csid ∈ [64, 319] → 2字节: fmt<<6 | 0, csid-64
csid ∈ [320, ...]→ 3字节: fmt<<6 | 1, (csid-64)&0xFF, (csid-64)>>8
```

**消息重组**：使用 `map<uint32_t, ChunkStream>` 为每个 csid 维护解析状态：
```cpp
struct ChunkStream {
    uint32_t timestamp, timestampDelta, length;
    uint8_t typeId;
    uint32_t streamId;
    ByteBuffer partial;  // 部分消息缓冲
};
```

---

## 四、Session 状态机

```
Handshaking → Connecting → Creating → Publishing → Ready
     │        │     │      │
 ▼     ▼            ▼       ▼
  Handshake    send connect   send        send publish
  C0C1C2S0S1S2  command     createStream   command
         ↓      ↓            ↓
      等待 _result    等待 _result  等待 onStatus
```

### connect 命令（AMF0 编码）
```
Command Name: "connect"
Transaction ID: 1
Command Object: {
    app: "live",
    tcUrl: "rtmp://host:1935/live",
    type: "nonprivate",
    flashVer: "FMLE/3.0"
}
```

### 协议控制消息处理
| 消息类型 | typeId | 处理 |
|----------|--------|------|
| Set Chunk Size | 1 | 更新 `chunkReader_.setChunkSize()` |
| Window Ack Size | 5 | 记录 `windowAckSize_` |
| Set Peer Bandwidth | 6 | 记录带宽限制 |
| Acknowledgement | 3 | 更新 `peerAckedBytes` → 通知 Metrics |

---

## 五、ACK 机制与 32 位回绕处理

### 发送 ACK
```cpp
// 每次 recv() 后累加
bytesReceived_ += receivedLen;

// 达到 windowAckSize/2 时发送 Acknowledgement
if (bytesReceived_ - lastAckSent_ >= windowAckSize_ / 2) {
    sendAck(bytesReceived_ & 0xFFFFFFFF);  // 只取低32位
    lastAckSent_ = bytesReceived_;
}
```

### 接收对端 ACK（回绕处理）
```cpp
void onPeerAck(uint32_t acked) {
    // 检测32位回绕：当 acked < lastPeerAck 且差值 > 2^31
    // 说明 acked 跨越了 0xFFFFFFFF → 0x00000000 边界
    if (acked < lastPeerAck_ && lastPeerAck_ - acked > 0x80000000U) {
    peerAckEpoch_ += (1LL << 32);  // epoch +1
    }
    lastPeerAck_ = acked;
    int64_t totalAcked = peerAckEpoch_ + acked;  // 真实确认量
    metrics_->onBytesAcked(totalAcked);
}
```

**为什么需要回绕处理**：RTMP Acknowledgement 字段是 uint32，最大 4GB。长时间推流（如 1Mbps × 9.5小时 ≈ 4GB）会溢出回绕。

---

## 六、三级背压策略

```
Normal ──(vqDelay≥800ms)──▶ Congested ──(vqDelay≥1200ms)──▶ DropGop
  ▲      │          │
  │              ▼          ▼
  │        通知编码器降码率30%      丢弃最老GOP + 请求IDR
  │     │           │
  │◀──────(vqDelay<400ms)────────┘◀──────(丢完后恢复)────────────┘
```

### 具体实现
```cpp
void PublisherImpl::applyBackpressure() {
    int64_t videoDelay = videoQueue_.frontDelay();  // 队首帧等待时间

    if (videoDelay >= dropGopThresholdMs_) {  // 1200ms
        // Level 3: 丢弃最老 GOP
        dropOldestGop();
     requestKeyframe();  // 通知编码器产生 IDR
        state_ = BackpressureState::DropGop;
    }
    else if (videoDelay >= maxQueueDelayMs_) {  // 800ms
        // Level 2: 通知上层降码率
        if (callback_) callback_->onCongestion(CongestionLevel::High);
        state_ = BackpressureState::Congested;
    }
    else if (videoDelay < recoveryThresholdMs_) {  // 400ms
        state_ = BackpressureState::Normal;
    }

    // 音频独立裁剪：超过 2000ms 直接丢最老帧
 while (audioQueue_.frontDelay() > maxAudioQueueMs_) {
 audioQueue_.pop();
    }
}
```

### GOP 感知丢弃
```cpp
void PublisherImpl::dropOldestGop() {
    // 丢到遇到下一个关键帧为止
    while (!videoQueue_.empty()) {
        auto& pkt = videoQueue_.front();
      bool isKeyframe = pkt.flags & FLAG_KEYFRAME;
        if (isKeyframe && droppedAtLeastOne) break;  // 保留下一个 GOP 的起始帧
        videoQueue_.pop();
droppedAtLeastOne = true;
        metrics_->onVideoDropped(1);
    }
}
```

---

## 七、断线重连

### 指数退避
```cpp
void PublisherImpl::scheduleReconnect() {
    backoffMs_ = (backoffMs_ == 0) ? reconnectBaseMs_    // 首次: 1000ms
     : min(backoffMs_ * 2, reconnectMaxMs_);  // ×2, 上限30s
    nextConnectAt_ = now + chrono::milliseconds(backoffMs_);
}
```

### 重连后恢复
```cpp
void PublisherImpl::onReconnected() {
    // 1. 重置时间基准
    baseDtsUs_ = 0;

 // 2. 重发 sequence headers (SPS/PPS + AudioSpecificConfig)
    if (videoSeqHeader_) session_->sendMediaMessage(videoSeqHeader_);
    if (audioSeqHeader_) session_->sendMediaMessage(audioSeqHeader_);

    // 3. 请求编码器产生关键帧
    requestKeyframe();

    // 4. 重置背压状态
    backoffMs_ = 0;
    state_ = BackpressureState::Normal;
}
```

---

## 八、Metrics（传输指标采集）

```cpp
class Metrics {
    atomic<int64_t> bytesSent_{0};
    atomic<int64_t> bytesAcked_{0};
    atomic<int> socketWriteBlockMs_{0};

    // EWMA 吞吐量计算（α=0.3，每500ms采样）
    void sampleThroughput() {
        int64_t delta = bytesSent_ - lastSampleBytes_;
 int instantBps = delta * 8 * 1000 / sampleIntervalMs;
 sendThroughputBps_ = 0.3 * instantBps + 0.7 * sendThroughputBps_;
    }

    Snapshot snapshot() {
        return {bytesSent_, bytesAcked_,
         bytesSent_ - bytesAcked_,  // inflight
                sendThroughputBps_, ...};
    }
};
```

---

## 九、IO 模型

### 单线程非阻塞事件循环
```cpp
void PublisherImpl::ioLoop() {
    while (running_) {
        // 1. 连接/重连逻辑
        if (state_ == Disconnected && now >= nextConnectAt_) {
            connectSession();
        }

        // 2. 驱动协议状态机
        session_->step();  // 内部: pumpRead → 处理收到的消息 → flush 发送缓冲

 // 3. 背压检测
      applyBackpressure();

        // 4. 排空队列（将音视频帧封装为 FLV tag 发送）
        drainQueues();

 // 5. 刷新发送缓冲
   session_->flush();  // transport_->send(out_buffer)

        // 6. 等待 socket 可读/可写（WSAPoll / poll，超时5ms）
   transport_->wait(5);
  }
}
```

**为什么单线程**：
- 避免多线程锁竞争（Session 状态、ChunkWriter 缓冲、Metrics 等共享状态）
- 推流场景下 IO 量可控，单线程 + 非阻塞足够
- 生产者线程通过无锁队列（BoundedQueue）与 IO 线程解耦

---

## 十、开发中遇到的问题与解决方案

### 问题 1：推流到 SRS 服务器连接后秒断

**现象**：Handshake 成功，connect 命令发出后服务器立即断开连接。

**原因**：connect 命令的 `tcUrl` 字段格式不对。SRS 严格要求 `tcUrl = "rtmp://host:port/app"`（不能带 stream name），而代码中错误地把完整 URL（包含 stream key）放入了 tcUrl。

**解决**：
```cpp
// 错误: tcUrl = "rtmp://host:1935/live/streamkey"
// 正确: tcUrl = "rtmp://host:1935/live", stream name 在 publish 命令中单独传
string tcUrl = "rtmp://" + host + ":" + port + "/" + app;
```

---

### 问题 2：推流 30 分钟后服务器断开连接

**现象**：稳定推流约 30 分钟后，服务器发送 Window Acknowledgement Size 但客户端未回应 ACK，服务器判定客户端失联后断开。

**原因**：忘记实现接收端 ACK 发送。RTMP 协议要求客户端在接收字节累计达到 `windowAckSize` 时发送 Acknowledgement 消息。

**解决**：在 `session_->pumpRead()` 中追踪 `bytesReceived_`，达到 `windowAckSize/2` 时主动发送 ACK 消息。

---

### 问题 3：弱网下重连后画面花屏

**现象**：断线重连恢复后，观看端画面出现马赛克/花屏。

**原因**：重连后直接发送 P 帧，但新的 TCP 连接意味着服务器/播放器缺少前序参考帧（SPS/PPS 和 IDR）。

**解决**：
1. 重连成功后首先重发 Video Sequence Header（SPS/PPS）
2. 重发 Audio Sequence Header（AudioSpecificConfig）
3. 请求编码器立即产生 IDR 帧
4. 丢弃重连前队列中的残留 P 帧

---

### 问题 4：chunk 分片后 nginx-rtmp 解析失败

**现象**：只有发送超过 4096 字节的视频帧时 nginx-rtmp 才报错。

**原因**：续包的 Basic Header 中 fmt 应该是 3（继承前一个 chunk），但代码中错误地每个续包都重新计算 fmt，导致某些续包用了 fmt=1/2，nginx 按不同逻辑解析导致长度错误。

**解决**：分片循环中，第一个 chunk 正常计算 fmt，后续 chunk 强制 fmt=3：
```cpp
for (size_t offset = 0; offset < payload.size(); offset += chunkSize_) {
    if (offset == 0) writeHeader(fmt, ...);  // 正常 fmt
    else writeHeader(3, csid);      // 强制 fmt=3
  writePayload(offset, min(chunkSize_, remaining));
}
```

---

### 问题 5：Extended Timestamp 导致某些播放器解析异常

**现象**：当 timestamp 累计超过 16777215（约 4.6 小时）后，某些播放器画面卡住。

**原因**：Extended Timestamp 在 fmt=3 续包中的处理不一致。部分服务器实现期望 fmt=3 的续包中**不重复**追加 extended timestamp，而代码中每个续包都追加了。

**解决**：遵循 RTMP 规范，对 fmt=3 的续包在 timestamp >= 0xFFFFFF 时也追加 extended timestamp（这是规范要求的），但同时提供一个兼容模式开关。经测试 SRS/nginx-rtmp/ZLMediaKit 均接受追加。

---

### 问题 6：高码率推流时 IO 线程 CPU 占用过高

**现象**：10Mbps 码率推流时，IO 线程 CPU 占用达 50%。

**原因**：每次 `drainQueues()` 只 pop 一个帧就回到循环顶部，而 `transport_->wait(5ms)` 每次都白等 5ms。高码率下帧间隔 < 5ms，导致频繁空等。

**解决**：`drainQueues()` 改为批量排空（一次循环中 pop 所有可用帧），减少 `wait()` 调用次数。同时 `wait()` 超时改为动态调整：队列非空时 0ms，队列空时 5ms。

---

## 十一、面试必须掌握的知识点

### 1. RTMP 协议基础
- **基于 TCP**，端口 1935
- **Chunk Stream**：大消息分片为小 chunk 传输，支持多路复用
- **Message Stream**：逻辑消息流，通过 streamId 区分
- **AMF0**：Action Message Format，用于序列化命令参数

### 2. 为什么自研而不用 librtmp？
| librtmp | 自研 |
|---------|------|
| 阻塞式 API，无法控制超时 | 非阻塞状态机，完全可控 |
| 无传输指标暴露 | 内嵌 Metrics（吞吐/RTT/inflight） |
| 无背压策略 | 三级背压 + GOP 感知丢帧 |
| 无 ACK 回绕处理 | 64位 epoch 扩展 |
| 难以集成断线重连 | 内建指数退避重连 |

### 3. Chunk 差分压缩的意义
- 音视频流中相邻 chunk 的 typeId、length、streamId 通常不变
- 只有 timestamp 在变，且 delta 通常恒定
- 用 fmt=3 可以把 12 字节头压缩到 1 字节，**节省约 3% 带宽**

### 4. 为什么 GOP 感知丢弃？
- P 帧依赖前序帧，不能随机丢
- 丢一个 P 帧 = 后续所有 P 帧都会花屏
- 必须丢弃整个 GOP（直到下一个 IDR），然后请求新 IDR

### 5. 背压设计哲学
- **队列是缓冲，不是垃圾桶**
- 无限排队 → 延迟无限增长 → 直播失去意义
- 三级递进：先警告（降码率）→ 再丢弃（保实时性）→ 最终降级

### 6. FLV Tag 格式
```
[TagType(1B)] [DataSize(3B)] [Timestamp(3B)] [TimestampExt(1B)] [StreamID(3B=0)] [Data...]

Video Tag Data:
  [FrameType(4bit)|CodecID(4bit)] [AVCPacketType(1B)] [CompositionTime(3B)] [NALU...]

Audio Tag Data:
  [SoundFormat(4bit)|Rate(2bit)|Size(1bit)|Type(1bit)] [AACPacketType(1B)] [AAC Data...]
```

### 7. Sequence Header
- **Video**: AVCDecoderConfigurationRecord（包含 SPS/PPS）
- **Audio**: AudioSpecificConfig（包含采样率/通道数/profile）
- 推流开始时必须先发，重连后也必须重发

### 8. 单线程非阻塞 vs 多线程
| 单线程非阻塞 | 多线程 |
|-------------|--------|
| 无锁竞争，确定性强 | 需要锁保护共享状态 |
| 调试容易，状态可追踪 | 死锁/竞态难以复现 |
| 适合 IO 密集型 | 适合 CPU 密集型 |
| Redis/Node.js 也是这个模型 | |
