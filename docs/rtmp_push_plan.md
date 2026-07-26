我已经掌握了足够的接入信息，直接给出方案。为避免反复被打断，我按最合理的默认假设来定：**核心层不依赖 Qt/FFmpeg，接受通用 H.264/AAC 数据；FFmpeg/AVPacket 通过独立适配层接入；首版 Windows 优先且网络后端可替换；里程碑上先打通协议与稳定发布，再叠加背压、重连和监控。** 如果某项假设你想改，告诉我即可微调。

下面是完整的可拆卸 RTMP 推流 SDK 方案。

---

# 一、设计目标与核心原则

## 1. 四条硬性目标

- **可拆卸**：核心库零 Qt、零 FFmpeg、零 CUDA 依赖，可直接放进任意 C++ 项目。
- **高性能**：编码线程零阻塞、零 `AVPacket` 负载拷贝、发送路径复用缓冲。
- **低延迟**：有界队列 + GOP 感知背压，队列时延可控。
- **可恢复**：Session 状态机 + 指数退避重连 + IDR 恢复。

## 2. 分层原则

```text
+----------------------------------------------+
|  接入层 (你的项目)  StreamController / MuxerManager        |
+------------------------------------+
|  适配层 rtmp_adapter (依赖 FFmpeg, 独立编译)               |
|   - AVPacket -> rtmp::MediaSample                |
|   - extradata -> SPS/PPS/AudioSpecificConfig              |
+------------------------------------+
|  SDK 公共 API 层 rtmp::Publisher (纯 C++/可选 C ABI)       |
+-----------------------------------------+
|  核心引擎层                                |
|   Session状态机 | 背压调度 | 恢复策略 | 指标采集           |
+-----------------------------------------+
|  协议层  Handshake | AMF0 | Chunk | Message封装            |
+------------------------------------+
|  传输层  ITransport (非阻塞Socket事件循环 / 后续IOCP)      |
+------------------------------------+
```

关键点：**上层只依赖 `rtmp::Publisher` 头文件，协议层和传输层完全隐藏在 SDK 内部。** 你的 `StreamController` 不需要知道 Chunk、AMF、Socket 的存在。

---

# 二、SDK 目录结构（独立于现有工程）

建议在仓库里新建一个平级独立目录，不混进现有 `.pro`：

```text
librtmp_push/
├── CMakeLists.txt              # 独立构建，产出静态/动态库
├── include/rtmp/               # 对外唯一暴露的头文件
│   ├── publisher.h             # 公共 API（核心）
│   ├── types.h                 # MediaSample / Config / 枚举
│   ├── stats.h                 # 指标结构
│   └── capi.h                  # 可选 C ABI（做 SDK 分发用）
├── src/
│   ├── core/
│   │   ├── publisher_impl.{h,cpp}      # 门面实现
│   │   ├── session.{h,cp}             # 状态机
│   │   ├── send_scheduler.{h,cpp}      # 背压 + 优先级调度
│   │   ├── recovery.{h,cpp}            # 重连 + IDR恢复
│   │   ├── bounded_queue.{h,cpp}       # 有界媒体队列
│   │   └── metrics.{h,cpp}             # 指标采集
│   ├── protocol/
│   │   ├── amf0.{h,cp}
│   │   ├── chunk_writer.{h,cpp}
│   │   ├── chunk_reader.{h,cpp}
│   │   ├── handshake.{h,cp}
│   │   ├── rtmp_message.{h,cpp}        # connect/publish/控制消息
│   │   ├── flv_tag.{h,cpp}             # H264/AAC -> RTMP payload
│   │   └── byte_buffer.{h,cpp}
│   ├── transport/
│   │   ├── itransport.h                # 抽象接口
│   │   ├── tcp_transport_select.cpp    # 首版：非阻塞 + WSAPoll
│   │   └── tcp_transport_iocp.cpp      # 后续：IOCP（不改上层）
│   └── util/
│       ├── loger.{h,cpp}
│       ├── ewma.h
│       └── spsc_ring.h
├── adapter_ffmpeg/             # 依赖 FFmpeg 的适配层，单独编译
│   ├── ffmpeg_bridge.{h,cp}   # AVPacket -> MediaSample
│   └── extradata_parser.{h,cpp}
└── tests/
    ├── test_amf0.cpp
    ├── test_chunk_fuzz.cpp     # 随机拆包一致性
    ├── test_handshake.cpp
    └── test_publish_local.cpp  # 对接 SRS/ZLM
```

这样最终你能得到：`rtmp_push.lib/.dll` + `include/rtmp/`，其他项目拷这两样即可用。

---

# 三、公共 API 设计（这是 SDK 的门面，最重要）

## 1. 数据契约 `types.h`

核心是一个**不含任何第三方类型**的媒体样本结构：

```cpp
namespace rtmp {

enum class Codec { H264, AAC };
enum class SampleType { VideoConfig, VideoKey, VideoInter, AudioConfig, AudioRaw };

// 一个待发送的媒体单元；data 所有权可选：外部持有或 SDK 接管
struct MediaSample {
    SampleType type;
    const uint8_t* data = nullptr;  // H264: AVCC(4字节长度前缀) ; AAC: raw
    size_t size = 0;
    int64_t dtsUs = 0;              // 统一微秒时基，SDK 内部再转 RTMP ms
    int64_t ptsUs = 0;
    bool ownData = false;           // true 时 SDK 负责释放
};

struct VideoParams { int width, height, fps; std::vector<uint8_t> sps, pps; };
struct AudioParams { int sampleRate, channels; std::vector<uint8_t> asc; }; // AudioSpecificConfig
```

设计要点：

- **统一微秒 `dtsUs/ptsUs`**，与你现有 `AVSyncClock` 一致，SDK 内部转 RTMP 的 ms 时间戳。
- **H.264 统一要求 AVCC 格式**（4 字节长度前缀），Annex B 由适配层转换。
- SPS/PPS/ASC 单独传入，SDK 内部生成 AVC/AAC Sequence Header，这样核心层无需解析码流。

## 2. 配置 `PublisherConfig`

```cpp
struct PublisherConfig {
    std::string url;                // rtmp://host:port/app/stream
    int chunkSize = 4096;
    // 背压
    int maxQueueDelayMs = 800;       // 超过则触发降级/丢帧
    int dropGopThresholdMs = 1200;   // 超过则 GOP 级丢弃 + 请求IDR
    int maxAudioQueueMs = 2000;
    // 重连
    int reconnectBaseMs = 1000;
    int reconnectMaxMs = 30000;
    int connectTimeoutMs = 5000;
    // 监控回调频率
    int statsIntervalMs = 1000;
};
```

## 3. 门面类 `Publisher`

```cpp
class Publisher {
public:
    explicit Publisher(const PublisherConfig&);
    ~Publisher();

    void setVideoParams(const VideoParams&);
    void setAudioParams(const AudioParams&);

    bool start();                    // 异步：立即返回，内部起 IO 线程
    void stop();

    // 编码线程调用：非阻塞入队，永不阻塞生产者
    // 返回 false 表示因背压被丢弃（已按策略处理）
    bool pushVideo(const MediaSample&);
    bool pushAudio(const MediaSample&);

    // 事件与指标
    void onState(std::function<void(SessionState, const std::string&)>);
    void onStats(std::function<void(const RtmpStats&)>);
    // 当 SDK 因背压需要编码器产生关键帧时回调（联动态码率/强制IDR）
    void onRequestKeyframe(std::function<void()>);
    void onRequestBitrate(std::function<void(int targetBps)>);
};
```

**关键设计：`pushVideo/pushAudio` 绝不阻塞**。它只做一件事：把样本引用放进有界队列，由独立 IO 线程消费。这就把网络阻塞和你的编码线程彻底解耦，解决了当前 `MuxerManager` 全局锁串行写导致的 Head-of-Line Blocking。

---

# 四、线程模型与内存所有权

## 1. 线程划分

```text
[编码线程(你的)] --pushVideo/Audio--> [有界队列] 
                                          |
                                   [IO/发送线程(SDK单线程)]
                                          |
                        Session状态机 + Chunk编码 + 非阻塞send +收ACK/控制
                          |
                                   [统计定时(可复用IO线程时钟)]
```

- **单 IO 线程**足够跑满单路 RTMP（协议本身是单连接串行），避免多线程锁竞争。
- IO 线程用一个事件循环：可写事件驱动发送，可读事件处理服务器控制消息/ACK，定时器驱动统计与 Ping。

## 2. 内存所有权（零负载拷贝）

这是"高性能"的关键。你现有链路是 `AVPacket*` 从对象池取出、入队、写完回收。SDK 里对应设计：

- `MediaSample.ownData` 决定所有权。适配层从 `AVPacket` 转 `MediaSample` 时，用 `av_packet_ref` 增加引用，`ownData=true`，并在 SDK 发送完成后通过回收回调 `av_packet_unref`。
- SDK 内部队列存的是**轻量描述符 + 数据指针**，不 memcpy 负载。
- 只有在 Chunk 分块拼头时，才对"头部"做小拷贝，负载用分散写（writev 风格）或按 chunkSize 切片引用。

内存所有权契约要在 API 文档里写死，否则跨项目复用最容易出跨模块 `new/delete` 崩溃。

---

# 五、协议层设计（技术含量最高的部分）

## 1. `ByteBuffer` + 增量解析

所有解析器都基于一个可增量喂入的缓冲：`append(data,n)` → 解析器尝试消费完整单元，不足则保留。这是应对 **TCP 粘包/拆包** 的统一手段，也是 fuzz 测试的基础。

## 2. Handshake

- 实现简单握手（C0/C1/C2、S0/S1/S2）。
- C1 的 time 字段填 0、zero 字段填 0、随机数填充；不做复杂握手（digest）首版够用，SRS/ZLM/nginx-rtmp 都接受简单握手。
- 状态机驱动：`SentC0C1 → RecvS0S1 → SentC2 → RecvS2 → Done`，全程非阻塞，超时可控。

## 3. AMF0 编解码

- 支持 Number/Bolean/String/Object/NullECMA Array/Object End。
- 发送 `connect` / `createStream` / `publish` / `@setDataFrame(onMetaData)`。
- 解析 `_result` / `_error` / `onStatus`，用 `transactionId` 关联请求响应。

## 4. Chunk Writer / Reader

- 写：实现 fmt=0/1/2/3 压缩，维护每个 CSID 的 prev 状态，支持 Extended Timestamp，按 chunkSize 切分。
- 读：重组跨 chunk 消息，处理 `Set Chunk Size`、`Window Ack Size`、`Set Peer Bandwidth`、`User Control`、`Acknowledgement`。
- CSID 分配约定：控制=2，命令(AMF)=3，音频=4，视频=6（与主流实现一致，便于抓包排查）。

## 5. FLV Tag 封装（H.264/AAC → RTMP payload）

- 视频：`[FrameType|CodecID][AVCPacketType][CompositionTime(3B)][AVCC NALU]`，`CompositionTime = (ptsUs-dtsUs)/1000`。
- 首帧发 AVC Sequence Header（由 SPS/PPS 组装 `AVCDecoderConfigurationRecord`）。
- 音频：`[SoundFormat|Rate|Size|Type][AACPacketType][raw]`，首帧发 AAC Sequence Header（ASC）。

面试可讲清 DTS/PTS/CompositionTime 三者关系，正是你现在禁用 B 帧但需理解的点。

---

# 六、有界队列 + GOP 感知背压（低延迟核心）

## 1. 队列度量：用媒体时长，不用包数

```text
queueDelayMs = (tailDtsUs - headDtsUs) / 1000
```

对视频、音频分别维护。字节数与媒体时长双指标。

## 2. 三级背压状态机

| 状态 | 触发 | 动作 |
|---|---|
| Normal | `queueDelay < maxQueueDelayMs` | 正常发送 |
| Congested | 超过 `maxQueueDelayMs` | 停止升码率，回调 `onRequestBitrate` 降码率 |
| DropGop | 超过 `dropGopThresholdMs` | 丢弃队列中"最旧完整 GOP 的未发送帧"，回调 `onRequestKeyframe` |

## 3. GOP 感知丢弃算法

队列里视频样本按 GOP 分组（`VideoKey` 开新组）。积压超阈值时：

1. 从队头找到最旧 GOP；
2. 丢弃该 GOP 尚未发送的 `VideoInter` 与后续，直到下一个 `VideoKey`；
3. **绝不丢已开始发送的 message 中途**（否则破坏 chunk 流）；
4. 音频保连续，仅在 `maxAudioQueueMs` 超限时成段丢最旧；
5. 触发 `onRequestKeyframe`，让你的 `VEncoder` 强制 IDR，从新关键帧恢复可解码链路。

这直接对应你 `dynamicbitratecontroller.cpp` 里现在只会"降 10%"的粗糙逻辑，升级成"降码率 + 丢旧 GOP + 请求 IDR"的组合拳。

---

# 七、协议级网络监控（替代当前不可靠的字节统计）

你现在的 `NetMonitor` 存在两个已确认的问题：自定义 AVIO 可能未真正发送、回调失败率不等于丢包。SDK 内建真实指标：

```cpp
struct RtmpStats {
    int64_t bytesSent;         // 实际 send() 成功字节
    int64_t bytesAcked;        // 服务器 Acknowledgement 确认字节
    int64_t bytesInflight;     // bytesSent - bytesAcked  ← 真实积压
    int   sendThroughputBps;   // 滑窗真实吞吐
    int   encodeInputBps;      // 生产码率（用于对比）
    int   rttMs;               // Ping/Pong 或 TCP_INFO
    int   videoQueueDelayMs;
    int   audioQueueDelayMs;
    int   socketWriteBlockMs;  // 单次可写等待耗时 P95
    int   droppedVideoFrames;
    int   requestedKeyframes;
    SessionState state;
};
```

信号平滑用 EWMA（`util/ewma.h`），拥塞判定用 `bytesInflight` 趋势 + RTT 梯度 + `queueDelay` 斜率，而非单一阈值。RTT 用 RTMP User Control Ping（首版）或 Windows `SIO_TCP_INFO`（增强）。

---

# 八、Session 状态机 + 断线 IDR 恢复

## 1. 状态

```text
Idle → Resolving → Connecting → Handshaking → AppConnecting
     → StreamCreating → Publishing → Streaming
     → (Error) → Backoff → Reconnecting → ...
     → Stopped
```

用**单一枚举 + 事件驱动**，杜绝 `isConected/isReconnecting/...` 布尔组合出非法态（这正是你 `NetworkMonitor` 现在的隐患）。

## 2. 断线判定（基于真实 Session）

不再用旁路 TCP 探测判断"服务器活着"。以真实连接信号为准：`send` 返回 0/`ECONNRESET`/`ETIMEDOUT`、长时间无 ACK 推进、`onStatus` error、长时间不可写。

## 3. 重连与恢复流程

```text
断线 → Backoff(指数退避+抖动: 1→2→4→...→30s)
     → 重新 handshake/connect/createStream/publish
     → 重发 onMetaData
     → 重发 AAC Sequence Header
     → 重发 AVC Sequence Header
     → 请求 IDR (onRequestKeyframe)
     → 从新关键帧恢复，时间戳基线归零
     → 期间推流队列丢旧数据，本地录制不受影响(录制在SDK之外)
```

时间戳首版采用"重连即新时间轴归零"，兼容性最好。

---

# 九、与你现有代码的接入方式（适配层）

你的 `StreamController` 目前是：`vEncoder_ → vPktQueue_ → videoMuxLoop → muxerManager_->writePacket`。接入 SDK 只需**在推流分支旁挂一个 Publisher**，录制仍走原 `Muxer`（天然实现录制/推流隔离）。

## 1. 适配层 `ffmpeg_bridge`

```cpp
// adapter_ffmpeg/ffmpeg_bridge.h （依赖 FFmpeg，单独编译）
rtmp::MediaSample fromVideoPacket(AVPacket* pkt, bool& isKey);
rtmp::MediaSample fromAudioPacket(AVPacket* pkt);
// 从 AVCodecContext->extradata 解析 SPS/PPS / ASC
rtmp::VideoParams videoParamsFrom(AVCodecContext*);
rtmp::AudioParams audioParamsFrom(AVCodecContext*);
```

注意：NVENC 输出可能是 Annex B，需在适配层转 AVCC；`extradata` 用 Fmpeg 的 `h264_mp4toannexb`/手动解析。

## 2. `StreamController` 改动点（最小侵入）

- `start()` 中若 `enableStream`，创建 `rtmp::Publisher`，`setVideoParams/AudioParams`（从编码器 extradata），`start()`。
- `videoMuxLoop/audioMuxLoop` 里，对推流分支不再走 `muxerManager` 的Push，而是 `publisher_->pushVideo(bridge::fromVideoPacket(pkt))`。
- `MuxerManager` 只保留 Record 输出，Push 输出移除。
- `onRequestKeyframe` → 调 `vEncoder_` 强制 IDR（需给 `VEncoder` 加一个 `forceIDR()`，NVENC 通过 `AV_PKT_FLAG` 或重设 `pict_type`）。
- `onRequestBitrate` → 复用你现有 `DynamicBitrateController`/`vEncoder_->setBitrate`。
- `onStats` → 喂给 `StatisticsDialog` 展示。

这样动态码率从"旁路 API 猜测"升级为"SDK 真实 inflight/ACK 驱动"。

---

# 十、构建与分发（做成 SDK）

-用 **CMake** 独立构建 `librtmp_push`，核心层只依赖标准库 + Winsock（`ws2_32`）。
- 产物：`rtmp_push.lib`（核心）+ `rtmp_push_ffmpeg.lib`（适配，可选）+ `include/rtmp/`。
- 提供 `capi.h`（C ABI）便于其它语言/项目接入，C++ 项目直接用 `Publisher`。
- 你的 `.pro` 里通过 `LIBS += -Lxx -lrtmp_push` 链接，`INCLUDEPATH += librtmp_push/include`。核心库不引入 Qt，避免污染。

---

# 十一、测试与验证（决定简历可信度）

1. **单元**：AMF0 往返、Chunk fmt0-3、Extended Timestamp、`Set Chunk Size` 动态生效。
2. **Fuzz**：把一段真实 RTMP 字节流随机切成 1..N 字节喂入解析器，断言输出恒等（验证拆包鲁棒性）。
3. **互操作**：对接 SRS / ZLMediaKit / nginx-rtmp，FFplay/VLC 拉流验证首帧、音画同步、长稳。
4. **弱网**：clumsy/自建限速代理，记录 `bytesInflight`、`queueDelay`、丢帧、恢复时间、Socket write P95。
5. **A/B**：`FFmpegRtmpPublisher`（现状）对比 `NativeRtmpPublisher`（自研），出首帧时延/弱网积压/重连时间对照表。

---

# 十二、里程碑拆解（按你"协议先行"的路线）

- **M0脚手架**：CMake、`types.h`、`Publisher` 空壳、`ByteBuffer`、日志。
- **M1 协议核心**：AMF0 + Chunk + Handshake + 单元/Fuzz 测试（不连网络）。
- **M2 打通发布**：TCP 非阻塞传输 + connect/createStream/publish + AVC/AAC Sequence Header + onMetaData，SRS/ZLM 能播。
- **M3 稳定发送**：单 IO 线程事件循环 + Partial Write + 有界队列（非阻塞 push）+ 适配层接入 `StreamController`，录制/推流隔离。
- **M4 可靠性**：Session 状态机 + ACK/Ping + 指数退避重连 + IDR 恢复。
- **M5 低延迟与监控**：GOP 感知背压 + inflight/RTT 趋势 + EWMA + 动态码率联动 + `RtmpStats` 回调。
- **M6 增强(可选)**：IOCP 传输后端替换、RTMPS(TLS)、自动化弱网压测脚本、C ABI 打包。

---

我可以把这个方案落成正式计划（含逐文件的任务清单和依赖顺序）。在那之前，先确认三点，避免方向偏差：

1. 平台首版是否就按 **Windows 优先 + 传输层可替换（先 WSAPoll，后 IOCP）**？
2. 输入契约是否采用 **通用 `MediaSample` + Fmpeg 适配层**（而非核心层直接吃 `AVPacket`）？
3. 里程碑是否就按 **M1 协议先行**？
