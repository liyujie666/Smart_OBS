## 1. RTMP协议完整解析

### 1.1 RTMP协议概述

**RTMP (Real-Time Messaging Protocol)** 是Adobe公司开发的专为音视频流媒体传输设计的应用层协议。

#### 基本特性

| 特性 | 说明 |
|------|------|
| 传输层 | 基于TCP，保证可靠有序传输 |
| 默认端口 | 1935 |
| 设计目标 | 低延迟直播（2-5秒） |
| 通信模式 | 全双工，支持推流和拉流 |
| 数据分块 | Chunk机制，支持多路复用 |

#### 协议分层架构

RTMP采用三层架构设计：

```
┌─────────────────────────────────────────┐
│  应用层 (Application Layer)             │
│  - 音视频数据                           │
│  - 元数据 (onMetaData)                  │
│  - 命令消息 (connect/publish)           │
├─────────────────────────────────────────┤
│  RTMP消息层 (Message Layer)             │
│  - 完整的逻辑消息                       │
│  - 消息类型：Audio/Video/Command/...    │
│  - 消息时间戳、类型ID、流ID             │
├─────────────────────────────────────────┤
│  RTMP块流层 (Chunk Stream Layer)        │
│  - 消息分块传输                         │
│  - 头部压缩                             │
│  - 多路复用                             │
├─────────────────────────────────────────┤
│  TCP传输层                              │
│  - 可靠传输                             │
│  - 流量控制                             │
└─────────────────────────────────────────┘
```

**设计意图**：

- **Message层**：提供应用层语义，定义完整的音视频/命令消息
- **Chunk层**：将大消息分块传输，实现多路复用，降低延迟
- **TCP层**：保证可靠传输

---

### 1.2 RTMP握手 (Handshake)

RTMP连接建立后的第一步是握手，用于版本协商和简单的身份验证。

#### 握手类型

- **简单握手 (Simple Handshake)**: 不进行加密，time和zero字段填0
- **复杂握手 (Complex Handshake)**: 包含HMAC-SHA256摘要验证，用于加密握手

**我们的SDK实现简单握手**，因为：

1. 主流RTMP服务器（SRS、ZLMediaKit、nginx-rtmp）都支持
2. 实现简单，调试方便
3. 直播场景通常不需要握手层加密

#### 握手流程图

```
Client                                    Server
  │                                         │
  │──────── C0 (1 byte) ───────────────────>│  版本号 = 3
  │──────── C1 (1536 bytes) ───────────────>│  时间戳+零+随机数
  │                                         │
  │<─────── S0 (1 byte) ────────────────────│  版本号 = 3
  │<─────── S1 (1536 bytes) ────────────────│  时间戳+零+随机数
  │                                         │
  │──────── C2 (1536 bytes) ───────────────>│  回显S1内容
  │                                         │
  │<─────── S2 (1536 bytes) ────────────────│  回显C1内容
  │                                         │
  │         握手完成，可以开始传输RTMP消息    │
```

#### 握手包格式详解

**C0 / S0 格式 (1字节)**

```
+--------+
|version |
+--------+
 1 byte
```

- **version**: RTMP协议版本号，固定为 `0x03` (版本3)

**C1 / S1 格式 (1536字节)**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        time (4 bytes)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        zero (4 bytes)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                   random bytes (1528 bytes)                   |
|                              ...                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**字段说明**:

- **time** (4字节): 时间戳，单位毫秒。简单握手填0
- **zero** (4字节): 保留字段，必须全为0
- **random** (1528字节): 随机数据

**C2 / S2 格式 (1536字节)**:

- **C2**: 客户端原样回显服务器的S1
- **S2**: 服务器原样回显客户端的C1

#### 代码实现示例

```cpp
void Handshake::start(ByteBuffer& out) {
    // C0: 版本号
    out.writeU8(0x03);
    // C1: time(4) + zero(4) + random(1528)
    c1_.resize(kHandshakeSize);  // 1536
    std::memset(c1_.data(), 0, 8);  // time和zero填0
    // 填充随机数
    for (int i = 8; i < kHandshakeSize; ++i) {
        c1_[i] = std::rand() & 0xFF;
    }
    out.write(c1_.data(), kHandshakeSize);
    state_ = State::WaitS0S1;
}
```

---

### 1.3 RTMP Chunk Stream (块流)

握手完成后，所有RTMP消息都通过**Chunk Stream**传输。这是RTMP协议的核心机制。

#### 为什么需要Chunk机制?

**问题场景**: 假设正在发送一个1MB的视频关键帧，如果作为单个包发送:

- 音频无法及时发送 → 音频卡顿
- 控制命令无法发送 → 响应延迟
- 大包重传代价高

**Chunk解决方案**:

1. **分块传输**: 将大消息切分成固定大小的块（默认128字节，推荐4096字节）
2. **多路复用**: 音频、视频、命令消息的chunk可以交错发送
3. **头部压缩**: 后续chunk省略重复头部，节省带宽
4. **低延迟**: 小块传输使得音频、视频、命令可以及时穿插

#### Chunk结构

```
+-------------+----------------+-------------------+--------------+
| Basic Header| Message Header |Extended Timestamp | Chunk Data   |
|  (1-3 bytes)|  (0/3/7/11 B)  |   (0 or 4 bytes)  | (<=chunkSize)|
+-------------+----------------+-------------------+--------------+
```

#### Basic Header (基本头)

**作用**: 指示chunk的格式类型(fmt)和块流ID(CSID)

**格式1 (1字节) - CSID范围 2-63**:

```
 0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+
|  fmt  | cs id |
+-+-+-+-+-+-+-+-+
  2bits   6bits
```

**格式2 (2字节) - CSID范围 64-319**:

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  fmt  |0 0 0 0|   cs id - 64  |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2bits   6bits      8bits
```

**格式3 (3字节) - CSID范围 64-65599**:

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  fmt  |0 0 0 1|     cs id - 64 (little endian)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2bits   6bits           16bits
```

**fmt字段 (2位)**: 决定Message Header的格式和长度

| fmt值 | 类型 | Message Header长度 | 使用场景 |
|-------|------|-------------------|----------|
| 0 | Type 0 | 11字节 | 新消息或时间戳回绕 |
| 1 | Type 1 | 7字节 | 同一流的新消息 |
| 2 | Type 2 | 3字节 | 同类型同长度消息 |
| 3 | Type 3 | 0字节 | 同一消息的后续chunk |

#### Message Header (消息头)

**Type 0 (fmt=0, 11字节) - 完整消息头**

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   timestamp (3 bytes)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                message length (3 bytes)       |message type id|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                message stream id (4 bytes, little endian)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **timestamp** (3字节): 绝对时间戳(ms)。如果>=0xFFFFFF，填0xFFFFFF，真实值放Extended Timestamp
- **message length** (3字节): 消息总长度
- **message type id** (1字节): 消息类型(Audio=8, Video=9等)
- **message stream id** (4字节，小端): 消息流ID

**Type 1 (fmt=1, 7字节) - 省略stream id**

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|              timestamp delta (3 bytes)                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                message length (3 bytes)       |message type id|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

- **timestamp delta** (3字节): 相对于前一消息的时间戳增量

**Type 2 (fmt=2, 3字节) - 只保留时间戳增量**

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        timestamp delta (3 bytes)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**Type 3 (fmt=3, 0字节) - 无消息头**

完全复用前一个chunk的所有信息。

#### Extended Timestamp (扩展时间戳)

当timestamp或timestamp delta >= 0xFFFFFF (16777215 ms ≈ 4.66小时) 时:

1. Message Header中的timestamp字段填 0xFFFFFF
2. 在Message Header之后追加4字节的Extended Timestamp

```
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                extended timestamp (4 bytes, big endian)       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

#### Chunk Stream ID (CSID) 约定

| CSID | 用途 | 消息类型 |
|------|------|----------|
| 2 | 协议控制消息 | Set Chunk Size, Acknowledgement等 |
| 3 | 命令和响应消息 | connect, createStream, publish等 |
| 4 | 音频数据 | Audio Message |
| 5 | 数据消息 | @setDataFrame (onMetaData) |
| 6 | 视频数据 | Video Message |

#### Chunk分块示例

**场景**: 发送500字节的视频消息，chunkSize=128

```
Chunk 1 (第1-128字节):
  [Basic Header fmt=0,csid=6][Message Header 11B][Data 128B]
Chunk 2 (第129-256字节):
  [Basic Header fmt=3,csid=6][Data 128B]
Chunk 3 (第257-384字节):
  [Basic Header fmt=3,csid=6][Data 128B]
Chunk 4 (第385-500字节):
  [Basic Header fmt=3,csid=6][Data 116B]
```

**关键点**:

- 第一个chunk用fmt=0发送完整头部
- 后续chunk用fmt=3，只有1字节Basic Header
- 接收端根据CSID重组，累积长度达到message length时完成接收

#### 头部压缩算法实现

```cpp
// chunk_writer.cpp 核心逻辑
void ChunkWriter::write(const RtmpMessage& msg, ByteBuffer& out) {
    uint32_t csid = msg.csid;
    PrevState& prev = prev_[csid];
    // 选择最优的fmt
    uint8_t fmt = 0;
    if (prev.valid) {
        if (msg.streamId == prev.streamId) {
            fmt = 1;  // 可省略streamId
            if (msg.length == prev.length && msg.typeId == prev.typeId) {
                fmt = 2;  // 可省略length和typeId
                uint32_t delta = msg.timestamp - prev.timestamp;
                if (delta == prev.timestampDelta) {
                    fmt = 3;  // 可省略所有头部
                }
            }
        }
    }
    // 分块传输
    size_t offset = 0;
    bool first = true;
    while (offset < msg.payload.size()) {
        if (!first) fmt = 3;  // 同一消息的后续块强制用fmt=3
        writeBasicHeader(out, fmt, csid);
        if (first) {
            writeMessageHeader(out, fmt, msg, prev);
            if (msg.timestamp >= 0xFFFFFF) {
                out.writeU32BE(msg.timestamp);  // Extended Timestamp
            }
        }
        size_t toWrite = std::min(chunkSize_, msg.payload.size() - offset);
        out.write(&msg.payload[offset], toWrite);
        offset += toWrite;
        first = false;
    }
    // 更新prev状态供下次压缩
    prev.timestamp = msg.timestamp;
    prev.timestampDelta = msg.timestamp - prev.timestamp;
    prev.length = msg.length;
    prev.typeId = msg.typeId;
    prev.streamId = msg.streamId;
    prev.valid = true;
}
```

---

### 1.4 RTMP消息类型

#### Message Type ID

| Type ID | 名称 | 说明 |
|---------|------|------|
| 1 | Set Chunk Size | 设置块大小(默认128，推荐4096) |
| 2 | Abort Message | 中止消息 |
| 3 | Acknowledgement | 确认已收到的字节数 |
| 4 | User Control Message | 用户控制消息(Ping/Pong等) |
| 5 | Window Acknowledgement Size | 设置确认窗口大小 |
| 6 | Set Peer Bandwidth | 设置对端带宽 |
| 8 | Audio | 音频数据 |
| 9 | Video | 视频数据 |
| 18 | Data Message (AMF0) | AMF0数据(onMetaData) |
| 20 | Command Message (AMF0) | AMF0命令(connect/publish) |

对应代码: `rtmp_message.h` 中的 `MessageType` 枚举。

---

### 1.5 RTMP推流完整流程

#### 连接建立序列

```
Client                                  Server
  |                                       |
  |------- TCP连接 ---------------------->|
  |                                       |
  |------- Handshake (C0+C1) ------------>|
  |<------ Handshake (S0+S1+S2) ----------|
  |------- C2 -------------------------->|
  |                                       |
  |--- SetWindowAckSize ----------------->|
  |--- SetChunkSize(4096) --------------->|
  |                                       |
  |--- connect("app") ------------------->| 连接到应用
  |<-- _result("connect success") --------|
  |                                       |
  |--- releaseStream("stream") ---------->| (可选)
  |--- FCPublish("stream") -------------->| (可选)
  |                                       |
  |--- createStream() ------------------->| 创建流
  |<-- _result(streamId=1) ---------------|
  |                                       |
  |--- publish("stream", "live") -------->| 发布流
  |<-- onStatus("publish start") ---------|
  |                                       |
  |--- @setDataFrame onMetaData --------->| 发送元数据
  |--- AAC Sequence Header -------------->| 音频配置
  |--- AVC Sequence Header -------------->| 视频配置
  |                                       |
  |--- Audio/Video Data ----------------->| 持续发送
  |--- Audio/Video Data ----------------->|
  |         ...                           |
  |                                       |
  |<-- Acknowledgement -------------------|服务器定期确认
  |                                       |
```

对应代码: `session.cpp` 的状态机流转。

### 1.6 核心命令详解

#### connect 命令

连接到RTMP应用，命令格式 (AMF0编码):

```
字段1: "connect" (String)
字段2: 1.0 (Number, transaction ID)
字段3: {  // 命令对象 (Object)
    app: "live",              // 从URL解析: rtmp://host/app/stream
    tcUrl: "rtmp://host:1935/live",
    type: "nonprivate",
    flashVer: "FMLE/3.0",
    fpad: false,
    audioCodecs: 3575,        // 支持的音频编解码器
    videoCodecs: 252,         // 支持的视频编解码器
    videoFunction: 1
}
```

服务器响应 `_result`:

```
字段1: "_result" (String)
字段2: 1.0 (Number, 匹配请求的transaction ID)
字段3: {  // 服务器属性
    fmsVer: "FMS/3,0,1,123",
    capabilities: 31
}
字段4: {  // 连接信息
    level: "status",
    code: "NetConnection.Connect.Success",
    description: "Connection succeeded"
}
```

#### createStream 命令

创建消息流:

```
字段1: "createStream" (String)
字段2: 2.0 (Number, transaction ID)
字段3: null (Null)
```

服务器响应:

```
字段1: "_result" (String)
字段2: 2.0 (Number)
字段3: null (Null)
字段4: 1.0 (Number, 分配的 stream ID)
```

#### publish 命令

发布流:

```
字段1: "publish" (String)
字段2: 0.0 (Number, transaction ID = 0)
字段3: null (Null)
字段4: "stream_name" (String)
字段5: "live" (String, 发布类型: live/record/append)
```

服务器响应 `onStatus`:

```
字段1: "onStatus" (String)
字段2: 0.0 (Number)
字段3: null (Null)
字段4: {
    level: "status",
    code: "NetStream.Publish.Start",
    description: "Stream is published"
}
```

对应代码: `session.cpp` 中的 `sendConnect()`、`sendCreateStream()`、`sendPublish()`。

---
