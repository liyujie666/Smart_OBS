对你这个项目，最推荐的深化方式是：

> **保留 FFmpeg 编解码能力，自己实现一个最小可用的 RTMP Publish Client，并在其上实现异步发送、协议级流控、低延迟背压和断线秒级恢复。**

这样既不是“只会调用 API”，又不会变成重复造整个 FFmpeg。

---

# 一、先说明当前 RTMP 实现存在的问题

目前推流核心还是：

```text
编码 AVPacket
    ↓
FFmpeg FLV Muxer
    ↓
av_interleaved_write_frame
    ↓
FFmpeg RTMP Protocol
    ↓
TCP Socket
```

这本身没问题，成熟项目也会使用 FFmpeg。但是当前你对发送层的观测并不可靠。

## 1. 自定义 AVIO 很可能没有真正工作

当前 `muxer/muxer.cpp` 中大致先做了：

```text
fmtCtx->pb = customAvioCtx
```

之后又调用：

```text
avio_open(&fmtCtx->pb, rtmpUrl, AVIO_FLAG_WRITE)
```

后面的 `avio_open` 很可能会覆盖前面设置的 `customAvioCtx`。

这意味着两种可能：

- 自定义回调根本没有被调用；
- 如果真的使用自定义回调，由于回调只返回 `buf_size`、没有真正发送数据，数据实际上会被直接丢弃。

所以第一步不是立即增加更多网络指标，而是先确认：

> 实际数据到底是由 FFmpeg 的 RTMP Protocol 发送，还是由自定义 AVIO 发送。

## 2. 当前“上传带宽”可能只是生产码率

`NetMonitor::addSendBytes()` 统计的是回调收到的字节数。如果回调位于封装层，而不是实际 Socket 发送层，那么得到的是：

> FFmpeg 生成数据的速率。

而不是：

> 网络实际成功发送的吞吐率。

两者在网络良好时接近，但在发生拥塞时可能完全不同。

真正需要分别统计：

- 编码器生产码率；
- FLV/RTMP 封装后码率；
- 进入发送队列的速率；
- Socket 实际写出速率；
- RTMP 对端确认速率；
- 发送队列积压字节；
- 单次 Socket Write 阻塞时间。

## 3. RTMP/TCP 不能直接统计应用层“丢包率”

RTMP 通常运行在 TCP 上。TCP 内部会重传丢失的数据包，因此应用层通常看不到“某个 RTMP 包丢了”。

`customWriteCallback` 返回失败次数不能直接定义为网络丢包率，因为一次写失败可能是：

- Socket 缓冲区满；
- 超时；
- 对端关闭；
- 本地网络切换；
- FFmpeg 内部错误；
- 自定义回调实现错误。

RTMP/TCP 推流更有意义的拥塞指标是：

- RTT；
- RTT 增长趋势；
- TCP 重传信息；
- Socket 写阻塞时间；
- 发送队列时延；
- RTMP Acknowledgement 推进速度；
- 实际吞吐；
- 服务端接收速率；
- 对端窗口大小。

## 4. 当前 RTT 测量方式不严谨

通过：

```text
av_write_frame(fmtCtx, nullptr)
```

测量调用耗时，不能直接认为是 RTT。

因为这个调用可能只是：

- 冲刷 FFmpeg 本地缓冲；
- 将数据写入内核 Socket Buffer；
- 没有等待对端响应。

真正的 RTMP RTT 可以基于：

- RTMP User Control `PingRequest/PingResponse`；
- TCP 层信息；
- 单独的服务端探测；
- RTMP Acknowledgement 推进时间。

因此，RTMP 深化的第一个阶段应该是先修正“真实可观测性”。

---

# 二、最能让面试官刮目相看的方向：实现最小 RTMP Publish Client

不建议从编解码器开始重写。你可以继续使用 FFmpeg 编码 H.264/AAC，但是自己实现 RTMP 传输层。

目标架构可以是：

```text
NVENC / AAC Encoder
          ↓
       AVPacket
          ↓
H.264/AAC 元数据提取
          ↓
RTMP Message Builder
          ↓
RTMP Chunk Encoder
          ↓
异步 TCP/TLS 发送层
          ↓
RTMP Server
```

你自己实现的内容包括：

1. RTMP 握手；
2. AMF0 编解码；
3. RTMP 命令交互；
4. RTMP Chunk 编解码；
5. H.264/AAC 到 RTMP 音视频消息的封装；
6. RTMP 控制消息；
7. 异步 Socket 发送；
8. 断线重连和关键帧恢复；
9. 协议级监控与流控。

这套实现非常有技术含量。

---

# 三、RTMP Publish Client 具体需要实现什么

## 1. RTMP 握手

RTMP 握手流程是：

```text
Client                         Server
  | ------ C0 + C1 ----------> |
  | <----- S0 + S1 + S2 ------ |
  | --------- C2 ------------> |
```

### 需要处理

- `C0`：RTMP 版本，一般为 3；
- `C1`：时间戳、随机数据；
- `S0/S1/S2`；
- `C2`；
- 握手超时；
- 分片接收；
- 部分发送；
- 服务器提前关闭；
- 简单握手与复杂握手兼容性。

这里真正体现能力的是：

> TCP 是字节流，一次 `recv` 不一定收到完整的 S0/S1/S2，因此必须设计增量解析器，而不能假设一次读取就是完整消息。

面试官很可能会问：

- TCP 是否有消息边界？
- 粘包拆包怎么处理？
- `send` 为什么可能只发送一部分？
- 如何设置握手超时？
- 握手期间断线如何处理？

这些都比单纯调用 `avio_open` 更有深度。

---

## 2. 实现 AMF0 编解码器

建立连接和发布流时，RTMP 使用 AMF0 编码命令。

至少需要支持：

- Number；
- Boolean；
- String；
- Object；
- Null；
- ECMA Array；
- Object End。

需要发送和解析的命令包括：

```text
connect
createStream
publish
```

典型交互过程：

```text
Client                         Server
  | ------ connect ----------> |
  | <----- _result ----------- |
  | ---- createStream -------> |
  | <----- _result(streamId) - |
  | ------ publish ----------> |
  | <----- onStatus ---------- |
```

需要维护 `transactionId`，根据 `_result` 把响应与请求对应起来。

这部分可以设计成：

```text
AmfValue
AmfEncoder
AmfDecoder
RtmpCommand
TransactionManager
```

面试时可以讲：

- 如何表示动态类型；
- 如何防止字符串长度越界；
- 如何处理恶意数据；
- 如何实现增量反序列化；
- 如何匹配异步请求和响应；
- 如何管理 Transaction ID。

---

## 3. 实现 RTMP Chunk 编解码

这是 RTMP 协议里最有技术含量的部分之一。

RTMP Message 会被切分成多个 Chunk 传输。需要实现：

- Basic Header；
- Message Header；
- Extended Timestamp；
- Chunk Stream ID；
- Chunk Size；
- Message Stream ID；
- 同一消息跨多个 Chunk 的重组；
- 多个 Chunk Stream 交错传输。

RTMP Header 有四种格式：

- `fmt=0`：完整消息头；
- `fmt=1`：省略 Message Stream ID；
- `fmt=2`：只携带时间戳差；
- `fmt=3`：复用之前的消息头。

需要为每个 Chunk Stream ID 保存历史状态，例如：

```text
timestamp
timestampDelta
messageLength
messageTypeId
messageStreamId
receivedBytes
payloadBuffer
```

### 这里面试官可能继续问

- 为什么 RTMP 要设计 Header Compression？
- `fmt=3` 如何恢复消息头？
- Extended Timestamp 什么时候出现？
- Chunk Size 改变后如何处理正在接收的消息？
- Message Stream ID 为什么是小端序，而其他字段通常是大端序？
- 如何防止超大 Message Length 导致内存攻击？
- 多个 Chunk Stream 为什么可以交错？

如果你能把这部分写清楚，基本不可能再被评价为“只会调 API”。

---

# 四、自己封装 H.264/AAC 到 RTMP Message

如果使用自己的 RTMP Client，就需要把编码器输出转换成 RTMP 音视频消息。

---

## 1. H.264 封装

需要理解两个常见格式：

### Annex B

通过起始码分割 NALU：

```text
00 00 00 01
00 00 01
```

### AVCC

每个 NALU 前面是长度：

```text
[NALU Length][NALU Data]
```

RTMP/FLV 中通常使用 AVCC 形式。

你需要完成：

- 判断编码器输出是 Annex B 还是 AVCC；
- 解析 NALU；
- 提取 SPS；
- 提取 PPS；
- 生成 `AVCDecoderConfigurationRecord`；
- 首次推流和参数变化后发送 AVC Sequence Header；
- 将普通帧打包成 AVC NALU Message；
- 判断 IDR；
- 设置 FrameType；
- 处理 DTS、PTS 和 Composition Time。

对于有 B 帧的视频：

\[
CTS = PTS - DTS
\]

RTMP Video Tag 中的 Composition Time 就是这个偏移。

你当前为了低延迟可能禁用了 B 帧，但仍然应该理解和支持这个字段。因为这是非常典型的面试考点：

- DTS 和 PTS 为什么不同？
- B 帧为什么导致解码顺序与显示顺序不同？
- Composition Time 为什么可能不为零？
- 推流为什么通常禁用 B 帧？

---

## 2. AAC 封装

AAC 需要处理：

- AudioSpecificConfig；
- AAC Sequence Header；
- AAC Raw Data；
- Sample Rate；
- Channel Configuration；
- AAC Object Type；
- ADTS Header 的解析或移除。

通常首次推流要先发送：

```text
AAC Sequence Header
```

之后再发送 AAC Raw Frame。

面试时可以解释：

- ADTS 和 AudioSpecificConfig 的区别；
- 为什么 RTMP 中通常不直接携带完整 ADTS；
- AAC 每帧 1024 个采样如何转换成时间戳；
- 48 kHz 下 AAC 帧时长为什么约为 21.33 ms。

---

## 3. 元数据

还可以使用 AMF0 发送 `onMetaData`：

- width；
- height；
- framerate；
- videocodecid；
- audiocodecid；
- audiosamplerate；
- audiochannels；
- video bitrate；
- audio bitrate；
- encoder 名称。

虽然实现难度不高，但它能让协议链路更完整。

---

# 五、RTMP 协议级流控：非常值得做

RTMP 不只是不断发送音视频数据，它还有自己的控制消息。

建议实现以下控制消息：

## 1. Set Chunk Size

动态调整 RTMP Chunk Size，例如从默认 128 调整为 4096 或更大。

可以测试不同 Chunk Size 对以下指标的影响：

- RTMP Header 开销；
- Socket 调用次数；
- 首帧延迟；
- 音视频交错及时性；
- 网络抖动下的表现。

注意 Chunk Size 并不是越大越好：

- Chunk 太小：协议头和系统调用开销较高；
- Chunk 太大：低优先级大消息可能阻塞其他消息；
- 音视频交错粒度降低；
- 弱网下单次积压更明显。

这可以形成一个很好的实验型亮点。

## 2. Window Acknowledgement Size

服务器会通知客户端确认窗口大小。

客户端统计收到的字节数，在达到窗口阈值后发送：

```text
Acknowledgement
```

反过来，你也可以基于服务端确认进度计算：

\[
B_{inflight} = B_{sent} - B_{acked}
\]

其中：

- \(B_{sent}\)：已经提交给 Socket 的累计字节；
- \(B_{acked}\)：服务端已经确认的累计字节。

它比单纯统计本地回调字节更接近真实链路状态。

如果 `inflight` 持续增长，说明：

- 客户端生产速度高于链路消化速度；
- 或 RTT 变大；
- 或服务端处理变慢；
- 或 TCP 缓冲发生积压。

## 3. Set Peer Bandwidth

解析：

- 窗口大小；
- Limit Type；
- Hard；
- Soft；
- Dynamic。

根据服务端提供的窗口约束发送速度。

## 4. User Control Message

实现：

- Stream Begin；
- Stream EOF；
- Ping Request；
- Ping Response。

可以基于 Ping Request/Ping Response 计算更可信的 RTMP 应用层 RTT。

---

# 六、比重写协议更实用的亮点：异步非阻塞发送架构

就算你实现了 RTMP 协议，如果最终还是在复用线程里同步 `send`，系统仍然容易卡顿。

建议设计：

```text
Encoder
   ↓
RTMP Media Queue
   ↓
RTMP Message Builder
   ↓
Chunk Encoder
   ↓
Priority Send Queue
   ↓
Non-blocking Socket Worker
```

## 1. 发送线程不能阻塞编码线程

编码线程只负责：

- 生成音视频数据；
- 打包成待发送对象；
- 快速入队。

网络线程负责：

- 分块；
- 部分发送；
- 重试 `WSAEWOULDBLOCK`；
- Socket 可写事件；
- 发送超时；
- 统计实际写出字节。

Windows 下可以逐步选择：

- `select`；
- `WSAPoll`；
- `WSAEventSelect`；
- IOCP。

如果想提高面试含金量，可以使用 IOCP。但不建议一开始直接上 IOCP，先把协议和状态机写正确。

---

## 2. 正确处理 Partial Write

TCP 的 `send` 返回值可能小于请求长度。例如准备发送 4096 字节，实际可能只写出 1200 字节。

每个待发送 Buffer 应维护：

```text
totalSize
sentOffset
enqueueTimestamp
mediaTimestamp
messageType
frameType
```

发送成功后：

```text
sentOffset += sentBytes
```

直到完整发送才出队。

这可以展示你真正理解：

> TCP 是可靠字节流，但一次 `send` 不保证发送完整 Buffer。

---

## 3. 优先级队列

控制消息不应该永远排在一个巨大视频关键帧后面。

可以设计优先级：

1. RTMP 控制消息；
2. AMF 命令消息；
3. AAC/AVC Sequence Header；
4. 音频帧；
5. 视频关键帧；
6. 视频普通帧。

但要防止完全打乱消息顺序。优先级调度必须建立在：

- 允许交错的 Chunk Stream；
- 同一 Message 内部 Chunk 顺序不可破坏；
- 同一媒体流 DTS 顺序不可破坏。

这很适合展示调度设计能力。

---

# 七、RTMP 推流最实用的深度：低延迟背压

网络带宽下降时，不能让 RTMP 发送队列无限增长。

## 1. 用媒体时长衡量队列

不能只看有多少个包。

可以计算：

\[
Q_{media} = DTS_{tail} - DTS_{head}
\]

例如发送队列里最旧视频包 DTS 是 10 秒，最新包 DTS 是 12 秒，那么发送队列已经积压约 2 秒。

也可以计算字节队列时延：

\[
Q_{delay} = \frac{8Q_{bytes}}{B_{send}}
\]

两个指标结合使用。

---

## 2. 设计水位状态机

例如：

```text
Normal
Warning
Congested
Recovering
```

示例策略：

### Normal

- 队列时延低于 150 ms；
- 正常推流。

### Warning

- 队列时延超过 150 ms；
- 暂停码率增长；
- 观察 RTT 和 ACK 进度。

### Congested

- 队列时延超过 400 ms；
- 立即降低视频码率；
- 停止非关键数据；
- 必要时执行 GOP 级丢弃。

### Recovering

- 队列时延恢复到低水位；
- 缓慢增加码率；
- 使用滞回避免状态反复切换。

---

## 3. GOP 感知丢帧

RTMP 发送队列积压严重时，不能随机丢 P 帧。

更合理的策略：

1. 发现延迟超过上限；
2. 丢弃旧 GOP 中尚未发送的视频帧；
3. 保留音频连续性；
4. 请求编码器生成新的 IDR；
5. 发送最新 SPS/PPS；
6. 从新的 IDR 恢复视频发送。

如果随便丢失参考帧，服务端和播放器可能直到下一个 IDR 才能恢复画面。

这部分可以包装为：

> 实现 GOP 感知的低延迟丢帧策略，在弱网积压时丢弃不可恢复的旧预测帧，并通过主动请求 IDR 快速恢复可解码视频链路。

这个点非常有实战价值。

---

# 八、断线重连做深，比“检测端口重新连接”有价值

现在你的网络监控主要是在旁路检查目标端口是否可连接。但是：

> 新建一个 TCP 探测连接成功，不代表原 RTMP 推流连接仍然有效。

真正的断线应该由实际 RTMP Session 判断：

- Socket 返回 0；
- `ECONNRESET`；
- `ETIMEDOUT`；
- 写操作持续失败；
- 长时间没有 ACK/Pong；
- 服务端返回 `onStatus` 错误；
- 服务端关闭 Stream。

---

## 1. RTMP Session 状态机

建议明确实现：

```text
Idle
Resolving
TcpConnecting
Handshaking
ConnectingApp
CreatingStream
Publishing
Streaming
Backoff
Reconnecting
Stopped
Failed
```

每个状态定义：

- 允许接收的事件；
- 超时时间；
- 失败跳转；
- 清理动作；
- 重试条件。

不能让几个布尔变量共同表示复杂状态，例如：

```text
isConnected
isReconnecting
isPublishing
isServerDisconnected
```

布尔组合容易出现非法状态。

---

## 2. 指数退避与随机抖动

重连间隔不要固定，可以使用：

\[
T_n = \min(T_{max}, T_0 \times 2^n) + J
\]

其中 \(J\) 是随机抖动。

例如：

```text
1s → 2s → 4s → 8s → 16s → 30s
```

这样可以避免服务端恢复时大量客户端同时重连造成惊群。

---

## 3. 重连期间如何处理媒体数据

这是面试官很可能会问的问题。

不应该把断线期间几秒钟的视频全部缓存起来，然后重连后高速发送，因为这会导致：

- 延迟巨大；
- 瞬时突发流量；
- 服务端缓存积压；
- 用户看到历史画面。

更合理的策略：

- 本地录制继续；
- 推流队列丢弃旧数据；
- 音视频编码可继续，也可以进入节流状态；
- 重连成功后清空旧推流数据；
- 重新发送元数据；
- 重新发送 AAC Sequence Header；
- 重新发送 AVC Sequence Header；
- 请求新 IDR；
- 从新的关键帧恢复；
- 重新建立推流时间戳基线。

---

## 4. 时间戳如何处理

RTMP 重连相当于一个新 Session。

可以选择：

### 方案一：时间戳重新从零开始

优点：

- 简单；
- 兼容性好；
- 新 Session 自然对应新时间轴。

### 方案二：保持逻辑连续时间戳

适用于服务端有特殊续流支持的情况，但兼容性更复杂。

普通 RTMP 重新 Publish 时，更推荐重新建立时间戳基线。

---

# 九、实现 RTMPS 可以增加安全方向深度

如果时间允许，可以增加 RTMPS：

```text
RTMP over TLS
```

自己不需要实现 TLS 算法，可以使用：

- OpenSSL；
- Windows SChannel。

重点实现：

- TLS Handshake；
- SNI；
- 证书链验证；
- 主机名验证；
- 证书过期处理；
- TLS Session Resumption；
- 安全地处理 `rtmp://` 和 `rtmps://`。

但 RTMPS 更像加分项，不建议优先于：

- RTMP Chunk；
- 异步发送；
- 背压；
- 重连恢复。

---

# 十、协议实现必须配套测试，否则容易变成“手写但不可靠”

建议为 RTMP Client 建立三层测试。

## 1. 单元测试

### AMF0

测试：

- 各数据类型编码和解码；
- 嵌套对象；
- 非法长度；
- 截断数据；
- 空字符串；
- 大字符串；
- 浮点数端序。

### Chunk

测试：

- fmt 0/1/2/3；
- 多 Chunk Message；
- Extended Timestamp；
- 多个 Chunk Stream 交错；
- 动态修改 Chunk Size；
- 数据被随机拆成 1～N 字节输入；
- 粘包输入；
- 截断输入；
- 超大 Message Length 拒绝。

特别推荐：

> 将一条 RTMP 字节流随机切成不同长度的小段，反复喂给增量解析器，验证输出结果始终一致。

这能有效验证 TCP 拆包处理。

## 2. 互操作测试

至少测试：

- SRS；
- ZLMediaKit；
- nginx-rtmp；
- FFplay；
- VLC。

验证：

- 能否完成握手；
- 能否 Publish；
- 首帧能否解码；
- 音视频是否同步；
- 长时间推流是否稳定；
- 不同 Chunk Size 是否兼容；
- 断线重连是否恢复。

## 3. 弱网测试

测试：

- 限制带宽；
- RTT 变化；
- 网络抖动；
- TCP Reset；
- 服务器重启；
- Wi-Fi 切换；
- 长时间 Socket 不可写；
- DNS 失败；
- TLS 证书错误。

记录：

- 首帧耗时；
- 重连耗时；
- 最大队列时延；
- 丢弃视频帧数；
- 音频连续性；
- 恢复到可解码画面的时间；
- P95/P99 Socket Write 耗时。

---

# 十一、推荐的实际落地方案

不建议一次性替换当前 FFmpeg 推流链路。建议做双后端：

```text
IRtmpPublisher
    ├── FFmpegRtmpPublisher
    └── NativeRtmpPublisher
```

## `FFmpegRtmpPublisher`

保留现有实现，用于：

- 稳定版本；
- 兼容性对照；
- 性能基准；
- 自研实现失败时回退。

## `NativeRtmpPublisher`

自己实现：

- TCP；
- RTMP 握手；
- AMF0；
- Chunk；
- Publish；
- 音视频消息封装；
- ACK/Ping；
- 重连；
- 异步发送。

这样你可以做 A/B 对比：

| 指标 | FFmpeg RTMP | Native RTMP |
|---|---:|---:|
| 首帧时间 | X ms | Y ms |
| 弱网最大队列延迟 | X ms | Y ms |
| 断线恢复时间 | X s | Y s |
| 发送线程 P99 阻塞 | X ms | Y ms |
| 内存峰值 | X MB | Y MB |
| 推流协议开销 | X% | Y% |

这比直接删掉 FFmpeg 实现更有工程意识。

---

# 十二、建议分五个阶段完成

## 第一阶段：修正现有 FFmpeg RTMP 链路

先完成：

- 核对自定义 AVIO 是否有效；
- 取消无效的“回调返回成功但不发送”；
- 区分生产码率和真实发送吞吐；
- 不再把回调失败率直接称为丢包率；
- 正确统计队列字节数；
- 统计 `av_interleaved_write_frame` 耗时；
- 为推流和录制建立独立输出线程。

即使最后不自研 RTMP，这一阶段也值得做。

## 第二阶段：实现协议基础库

实现：

- Byte Reader/Writer；
- AMF0；
- RTMP Chunk Encoder；
- RTMP Chunk Decoder；
- 增量输入 Buffer；
- 单元测试。

先测试协议编解码，不要急着连服务器。

## 第三阶段：打通 Publish

实现：

- TCP 连接；
- Handshake；
- `connect`；
- `createStream`；
- `publish`；
- H.264 Sequence Header；
- AAC Sequence Header；
- 音视频消息；
- Metadata。

目标是能被 ZLMediaKit/SRS 正常播放。

## 第四阶段：实现可靠性

实现：

- Session 状态机；
- 超时；
- ACK；
- Ping/Pong；
- Partial Write；
- 异步发送；
- 指数退避重连；
- IDR 恢复；
- 推流与录制隔离。

## 第五阶段：实现低延迟和拥塞控制

实现：

- 发送队列媒体时长；
- ACK 推进速率；
- Socket 写阻塞；
- RTT 趋势；
- GOP 感知丢帧；
- 动态码率联动；
- 自动弱网测试。

---

# 十三、简历上怎么写

完成基本协议后，可以写：

> 基于 C++ 实现 RTMP Publish Client，完成 C0/C1/C2 握手、AMF0 命令编解码、Chunk Stream 分片重组及 `connect/createStream/publish` 状态机，并将 NVENC H.264 与 AAC 编码数据封装为 RTMP 音视频消息，与 SRS/ZLMediaKit 完成互操作。

完成异步发送后，可以写：

> 设计非阻塞 RTMP 发送管线，支持 TCP Partial Write、优先级 Chunk 调度和有界媒体队列，将网络 I/O 与编码/本地录制线程隔离，避免弱网阻塞传播至媒体生产链路。

完成协议流控后，可以写：

> 实现 RTMP Window Acknowledgement、Set Peer Bandwidth 与 Ping/Pong 控制消息，基于发送字节、对端 ACK 进度、Socket 写阻塞及队列媒体时长构建协议级链路监控，替代仅依赖服务端 API 的旁路网络检测。

完成断线恢复后，可以写：

> 设计 RTMP Session 状态机与指数退避重连机制，重连后自动重发 Metadata、AAC/AVC Sequence Header 并请求 IDR，从最新可解码 GOP 恢复推流，使断网重连至首个可播放画面的耗时控制在 X 秒以内，同时保证本地录制不中断。

完成拥塞控制后，可以写：

> 实现 GOP 感知的低延迟背压策略，在弱网积压时丢弃旧预测帧并从新 IDR 恢复，结合 RTMP ACK 推进速率和发送队列时延驱动动态码率，将 X Mbps→Y Mbps 带宽突降下的推流队列峰值延迟由 X ms 降至 Y ms。

---

# 十四、面试时最有冲击力的讲法

不要说：

> 我自己实现了 RTMP 协议。

这样太笼统，面试官会立即问你是不是照着文档复制。

应该说：

> 最初项目直接使用 FFmpeg RTMP 输出，但我发现封装回调统计到的只是数据生产速率，无法反映 TCP 实际发送能力，而且同步网络写入会把阻塞传播到录制链路。因此我保留 FFmpeg 作为对照后端，另外实现了最小 RTMP Publisher：包括增量握手、AMF0 Transaction、Chunk Header Compression、Window ACK 和异步 Partial Write，并以发送队列媒体时长与 ACK 推进速度作为拥塞信号。在弱网积压时按 GOP 丢弃旧视频数据，主动请求 IDR 恢复，保证本地录制不受推流故障影响。

这段话里面同时包含：

- 发现问题；
- 协议理解；
- 网络编程；
- 系统解耦；
- 拥塞反馈；
- 编码知识；
- 故障恢复；
- 对照验证。

面试官自然就不会把项目理解成简单调用 API。

---

# 十五、如果时间有限，只做哪一部分

如果你只能投入一段有限时间，不建议一上来完整手写 RTMP。

最佳的三个投入档位是：

## 小投入：推流输出隔离与可观测性

完成：

- 独立 RTMP Worker；
- 有界发送队列；
- 实际写入耗时；
- 队列字节数和媒体时长；
- 推流阻塞不影响录制；
- 重连后 IDR 恢复。

这是工程收益最高的方案。

## 中等投入：最小 RTMP Publisher

完成：

- Handshake；
- AMF0；
- Chunk；
- Publish；
- H.264/AAC Message；
- Ping/ACK；
- 基础重连。

这是最适合写进简历的方案。

## 大投入：自研低延迟 RTMP 传输引擎

在中等投入基础上增加：

- IOCP 异步发送；
- 优先级 Chunk 调度；
- ACK 驱动流控；
- GOP 感知背压；
- 动态码率联动；
- RTMPS；
- 自动弱网压测；
- FFmpeg 与自研后端 A/B 基准。

这是最能作为项目核心亮点的方案。

---

## 最终推荐

如果目标是让面试官真正刮目相看，我最推荐你把 RTMP 方向定义为：

> **自研最小 RTMP Publish Client + 协议级网络监控 + GOP 感知低延迟背压 + 断线关键帧恢复。**

其中优先顺序是：

1. 先修正当前自定义 AVIO 和网络指标；
2. 将录制与推流输出解耦；
3. 实现 RTMP Handshake、AMF0 和 Chunk；
4. 打通 H.264/AAC 发布；
5. 实现 ACK/Ping 和异步发送；
6. 实现有界队列、GOP 丢帧和 IDR 恢复；
7. 最后把真实指标接入动态码率控制器。
