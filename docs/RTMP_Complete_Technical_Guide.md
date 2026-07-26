# RTMP推流SDK完整技术指南

> 本文档系统性讲解RTMP推流SDK的核心技术原理，涵盖协议细节、编码格式、网络传输、流控策略等方方面面。
> 
> 适合人群：需要深入理解RTMP协议实现细节的开发者

---

## 目录

1. [RTMP协议完整解析](#1-rtmp协议完整解析)
2. [FLV封装格式详解](#2-flv封装格式详解)
3. [H.264视频编码格式](#3-h264视频编码格式)
4. [AAC音频编码格式](#4-aac音频编码格式)
5. [AMF0数据编码](#5-amf0数据编码)
6. [网络传输与非阻塞IO](#6-网络传输与非阻塞io)
7. [时间戳与音视频同步](#7-时间戳与音视频同步)
8. [状态机与会话管理](#8-状态机与会话管理)
9. [背压控制与流控](#9-背压控制与流控)
10. [错误处理与重连策略](#10-错误处理与重连策略)

---

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

## 2. FLV封装格式详解

### 2.1 FLV概述

RTMP传输音视频时使用 **FLV (Flash Video)** 格式封装。虽然不需要生成完整的FLV文件，但音视频消息的payload必须符合FLV Tag格式。

**关键点**:
- RTMP的Audio/Video消息的payload就是FLV Tag Data部分
- 不包含FLV文件头和Tag Header
- 只需要关注Tag Data的内部格式

### 2.2 FLV视频Tag格式

#### 视频Tag结构

```
+--------+--------+--------+--------+--------+-----...-----+
|FrameTyp|  AVC   |   Composition Time    |     Data      |
|CodecID | Packet |      (3 bytes)        |               |
|        |  Type  |                       |               |
+--------+--------+--------+--------+--------+-----...-----+
   1B       1B            3B                   变长
```

#### 第1字节: FrameType + CodecID

```
 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+
|FrameTyp|CodecID
+-+-+-+-+-+-+-+-+
  4 bits  4 bits
```

**FrameType (高4位)**:

| 值 | 类型 | 说明 |
|----|------|------|
| 1 | keyframe | IDR关键帧 |
| 2 | inter frame | 非关键帧(P帧) |
| 3 | disposable inter frame | 可丢弃的帧(B帧) |
| 4 | generated keyframe | 服务器生成的关键帧 |
| 5 | video info/command | 视频信息/命令帧 |

**CodecID (低4位)**:

| 值 | 编码器 |
|----|--------|
| 2 | Sorenson H.263 |
| 3 | Screen video |
| 4 | On2 VP6 |
| 5 | On2 VP6 with alpha |
| 6 | Screen video v2 |
| 7 | AVC (H.264) |

**典型值**:
- `0x17` (0001 0111): 关键帧 + H.264
- `0x27` (0010 0111): 非关键帧 + H.264

对应代码:
```cpp
// flv_tag.cpp
uint8_t byte0 = (keyframe ? 0x10 : 0x20) | 0x07;  // FrameType | CodecID
```

#### 第2字节: AVCPacketType

| 值 | 类型 | 说明 |
|----|------|------|
| 0 | AVC sequence header | 解码器配置(SPS/PPS) |
| 1 | AVC NALU | 实际视频帧数据 |
| 2 | AVC end of sequence | 序列结束(可选) |

**使用场景**:
- **首帧**: AVCPacketType=0，发送SPS/PPS
- **后续帧**: AVCPacketType=1，发送NALU数据

#### 第3-5字节: CompositionTime (CTS)

```
CompositionTime = PTS - DTS (单位: 毫秒)
```

**有符号24位整数**，大端字节序。

**计算示例**:
```cpp
int32_t cts = (ptsUs - dtsUs) / 1000;  // 微秒转毫秒

// 转换为24位有符号整数
if (cts < 0) {
    cts = (1 << 24) + cts;  // 负数用补码表示
}

byte3 = (cts >> 16) & 0xFF;
byte4 = (cts >> 8) & 0xFF;
byte5 = cts & 0xFF;
```

**注意事项**:
- 无B帧时，PTS=DTS，CompositionTime=0
- B帧场景中，CompositionTime可以为负数(显示顺序在解码顺序之前)
- 直播推流通常禁用B帧，简化处理

#### Data部分

**AVC Sequence Header (AVCPacketType=0)**:

这就是 **AVCDecoderConfigurationRecord** 格式:

```
+--------+--------+--------+--------+--------+--------+
|  0x01  | profile| compat | level  | 0xFF   | 0xE1   |
+--------+--------+--------+--------+--------+--------+
|  SPS length (2B, big endian)     |   SPS data ...  |
+-----------------------------------+-----------------+
| 0x01   | PPS length (2B)          |  PPS data ...   |
+--------+--------------------------+-----------------+
```

**字段说明**:
- **configurationVersion**: 固定为1
- **AVCProfileIndication**: H.264 profile (Baseline=66, Main=77, High=100)
- **profile_compatibility**: 兼容性标志
- **AVCLevelIndication**: H.264 level (如3.1=31, 4.0=40)
- **lengthSizeMinusOne**: NALU长度字段大小-1，通常为3 (4字节长度) → 0xFF & 0x03 = 0xFF
- **numOfSequenceParameterSets**: SPS数量，通常为1 → 0xE1 & 0x1F = 0x01
- **SPS length**: SPS长度(2字节，大端)
- **SPS data**: SPS数据
- **numOfPictureParameterSets**: PPS数量，通常为1
- **PPS length**: PPS长度(2字节)
- **PPS data**: PPS数据

**代码实现**:
```cpp
// flv_tag.cpp: buildAvcSequenceHeader()
std::vector<uint8_t> buildAvcSequenceHeader(
    const std::vector<uint8_t>& sps,
    const std::vector<uint8_t>& pps)
{
    std::vector<uint8_t> config;
    
    config.push_back(0x01);  // configurationVersion
    config.push_back(sps[1]); // AVCProfileIndication (从SPS提取)
    config.push_back(sps[2]); // profile_compatibility
    config.push_back(sps[3]); // AVCLevelIndication
    config.push_back(0xFF);   // lengthSizeMinusOne (4字节长度)
    config.push_back(0xE1);   // numOfSequenceParameterSets = 1
    
    // SPS length + data
    config.push_back((sps.size() >> 8) & 0xFF);
    config.push_back(sps.size() & 0xFF);
    config.insert(config.end(), sps.begin(), sps.end());
    
    config.push_back(0x01);   // numOfPictureParameterSets = 1
    
    // PPS length + data
    config.push_back((pps.size() >> 8) & 0xFF);
    config.push_back(pps.size() & 0xFF);
    config.insert(config.end(), pps.begin(), pps.end());
    
    return config;
}
```

**AVC NALU (AVCPacketType=1)**:

NALU数据必须是 **AVCC格式** (4字节长度前缀 + NALU数据，不含start code):

```
+--------+--------+--------+--------+-----...-----+
|      NALU length (4 bytes)       |  NALU data  |
+--------+--------+--------+--------+-----...-----+
|      NALU length (4 bytes)       |  NALU data  |
+--------+--------+--------+--------+-----...-----+
```

**多个NALU的情况**:
一帧可能包含多个NALU(如一个SEI + 一个IDR slice)，每个NALU都有独立的4字节长度前缀。

**代码示例**:
```cpp
// 假设 avccData 已经是AVCC格式
std::vector<uint8_t> buildVideoTag(
    const uint8_t* avccData, size_t size,
    bool keyframe, int32_t compositionTimeMs)
{
    std::vector<uint8_t> tag;
    
    // Byte 1: FrameType | CodecID
    tag.push_back((keyframe ? 0x10 : 0x20) | 0x07);
    
    // Byte 2: AVCPacketType = 1 (NALU)
    tag.push_back(0x01);
    
    // Byte 3-5: CompositionTime (24-bit signed, big endian)
    int32_t cts = compositionTimeMs;
    if (cts < 0) cts = (1 << 24) + cts;
    tag.push_back((cts >> 16) & 0xFF);
    tag.push_back((cts >> 8) & 0xFF);
    tag.push_back(cts & 0xFF);
    
    // Data: AVCC NALUs
    tag.insert(tag.end(), avccData, avccData + size);
    
    return tag;
}
```

---

### 2.3 FLV音频Tag格式

#### 音频Tag结构

```
+--------+--------+-----...-----+
|SoundFmt|  AAC   |    Data     |
|RateSzTy| Packet |             |
|        |  Type  |             |
+--------+--------+-----...-----+
   1B       1B        变长
```

#### 第1字节: SoundFormat + SoundRate + SoundSize + SoundType

```
 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+-+
|SndFmt |R|Sz|Tp|
+-+-+-+-+-+-+-+-+
 4 bits 2b 1b 1b
```

**SoundFormat (高4位)**:

| 值 | 格式 |
|----|------|
| 0 | Linear PCM, platform endian |
| 1 | ADPCM |
| 2 | MP3 |
| 3 | Linear PCM, little endian |
| 6 | Nellymoser 8kHz mono |
| 10 | AAC |
| 11 | Speex |

**SoundRate (第5-6位)**:

| 值 | 采样率 |
|----|--------|
| 0 | 5.5 kHz |
| 1 | 11 kHz |
| 2 | 22 kHz |
| 3 | 44 kHz |

**SoundSize (第7位)**:

| 值 | 位深度 |
|----|--------|
| 0 | 8-bit |
| 1 | 16-bit |

**SoundType (第8位)**:

| 值 | 声道 |
|----|------|
| 0 | Mono |
| 1 | Stereo |

**典型值**:
- **AAC 44.1kHz 16-bit Stereo**: `0xAF` (1010 1111)
  - SoundFormat=10 (AAC)
  - SoundRate=3 (44kHz)
  - SoundSize=1 (16-bit)
  - SoundType=1 (Stereo)

**注意**: AAC格式时，SoundRate/SoundSize/SoundType的实际值由AudioSpecificConfig决定，此处填固定值即可。

#### 第2字节: AACPacketType

| 值 | 类型 | 说明 |
|----|------|------|
| 0 | AAC sequence header | AudioSpecificConfig |
| 1 | AAC raw | 实际音频帧数据 |

#### Data部分

**AAC Sequence Header (AACPacketType=0)**:

包含 **AudioSpecificConfig (ASC)**，通常2字节:

```
 0                   1
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|audioObjectType|samplingFreqIdx|
|   (5 bits)    |channelConfig  |
|               |   (4 bits)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

**字段说明**:
- **audioObjectType** (5位): AAC类型
  - 1 = AAC Main
  - 2 = AAC LC (Low Complexity，最常用)
  - 5 = AAC HE (High Efficiency)
- **samplingFrequencyIndex** (4位): 采样率索引
- **channelConfiguration** (4位): 声道配置
  - 1 = Mono
  - 2 = Stereo
- **frameLengthFlag** (1位): 通常为0
- **dependsOnCoreCoder** (1位): 通常为0
- **extensionFlag** (1位): 通常为0

**采样率索引表**:

| Index | 采样率 (Hz) |
|-------|------------|
| 0x0 | 96000 |
| 0x1 | 88200 |
| 0x2 | 64000 |
| 0x3 | 48000 |
| 0x4 | 44100 |
| 0x5 | 32000 |
| 0x6 | 24000 |
| 0x7 | 22050 |
| 0x8 | 16000 |
| 0x9 | 12000 |
| 0xa | 11025 |
| 0xb | 8000 |

**计算示例 (AAC-LC, 44.1kHz, Stereo)**:

```
audioObjectType = 2 (AAC-LC)
samplingFrequencyIndex = 4 (44100 Hz)
channelConfiguration = 2 (Stereo)

Byte 1:
  bits 0-4: audioObjectType = 2 = 00010
  bits 5-7: samplingFreqIdx 高3位 = 010
  → 00010010 = 0x12

Byte 2:
  bit 0: samplingFreqIdx 低1位 = 0
  bits 1-4: channelConfiguration = 2 = 0010
  bits 5-7: flags = 000
  → 00010000 = 0x10

ASC = [0x12, 0x10]
```

**代码实现**:
```cpp
// flv_tag.cpp: buildAacSequenceHeader()
std::vector<uint8_t> buildAacSequenceHeader(
    const std::vector<uint8_t>& asc,
    int sampleRate, int channels)
{
    if (!asc.empty()) {
        return asc;  // 如果已有ASC，直接使用
    }
    
    // 否则根据参数构建
    int audioObjectType = 2;  // AAC-LC
    int samplingFreqIndex = getSamplingFreqIndex(sampleRate);
    int channelConfig = channels;
    
    uint8_t byte1 = (audioObjectType << 3) | (samplingFreqIndex >> 1);
    uint8_t byte2 = ((samplingFreqIndex & 0x1) << 7) | (channelConfig << 3);
    
    return {byte1, byte2};
}
```

**AAC Raw (AACPacketType=1)**:

直接跟随 AAC 原始数据 (ADTS header已去除，只有AAC Access Unit)。

**注意**: 如果编码器输出的是ADTS格式(包含7字节同步头)，需要在适配层去掉头部:

```cpp
// 跳过ADTS头(通常7字节，有时9字节)
int adtsHeaderSize = 7;
if ((adtsFrame[1] & 0x01) == 0) {
    adtsHeaderSize = 9;  // 有CRC
}
const uint8_t* rawAAC = adtsFrame + adtsHeaderSize;
size_t rawSize = frameSize - adtsHeaderSize;
```

对应代码: `flv_tag.cpp` 的 `buildAudioTag()`。

---

## 3. H.264视频编码格式

### 3.1 H.264基础概念

**H.264** (也称MPEG-4 AVC) 是目前最广泛使用的视频编码标准。

#### NAL Unit (网络抽象层单元)

H.264码流由一系列**NALU**组成，每个NALU是一个独立的编码单元。

**NALU Header** (第1字节):
```
 7 6 5 4 3 2 1 0
+-+-+-+-+-+-+-+
|F|NRI|  Type   |
+-+-+-+-+-+-+
 1b 2b   5bits
```

- **F** (forbidden_zero_bit): 必须为0
- **NRI** (nal_ref_idc): 重要性指示(0-3)，参考帧为非0值
- **Type** (nal_unit_type): NALU类型(低5位)

#### NALU类型

| Type | 名称 | 说明 | 重要性 |
|------|------|-----|
| 1 | Non-IDR Slice | 非IDR帧的slice | 参考帧NRI>0 |
| 5 | IDR Slice | IDR关键帧的slice | NRI=3 |
| 6 | SEI | 补充增强信息 | NRI=0 |
| 7 | SPS | 序列参数集 | NRI=3 |
| 8 | PPS | 图像参数集 | NRI=3 |
| 9 | AUD | 访问单元分隔符 | NRI=0 |

#### SPS和PPS详解

**SPS (Sequence Parameter Set)**:
- 包含：分辨率、帧率、profile、level、长宽比等
- 作用域：整个视频序列
- 变化时机：分辨率改变、编码器重新初始化

**PPS (Picture Parameter Set)**:
- 包含：熵编码模式、量化参数、去块滤波器参数等
- 作用域：一组图像
- 通常每个SPS对应一个或多个PPS

**关键点**:
- SPS和PPS必须在第一个IDR帧之前发送
- RTMP中通过**AVC Sequence Header**发送
- 中途改变分辨率需要重新发送新的SPS/PPS

### 3.2 Annex B vs AVCC 格式

#### Annex B 格式 (字节流格式)

用于文件存储(.264文件)和直播传输(TS流)，每个NALU前有start code:

```
00 00 00 01 [NALU1] 00 00 00 01 [NALU2] 00 01 [NALU3] ...
```

- **4字节start code**: `00 00 00 01`
- **3字节start code**: `00 00 01` (后续NALU可用)

**示例**:
```
00 00 00 01 67 ... (SPS)
00 00 00 01 68 ... (PS)
00 00 00 01 65 ... (IDR slice)
00 00 01 41 ... (P slice)
```

#### AVCC 格式 (长度前缀格式)

用于MP4、FLV等容器，每个NALU前是4字节长度:

```
[length(4B)][NALU1][length(4B)][NALU2]...
```

**示例**:
```
00 00 00 1A 67 ... (length=26, SPS)
00 00 00 08 68 ... (length=8, PPS)
00 00 05 3C 65 ... (length=1340, IDR slice)
```

**RTMP要求AVCC格式**。

#### 格式转换

**Annex B → AVCC**:
```cpp
std::vector<uint8_t> annexbToAvcc(const uint8_t* data, size_t size) {
    std::vector<uint8_t> avcc;
    size_t pos = 0;
    
    while (pos < size) {
        // 查找start code
        int startCodeLen = 0;
        if (pos + 3 < size && data[pos] == 0 && data[pos+1] == 0) {
            if (data[pos+2] == 1) {
                startCodeLen = 3;
            } else if (data[pos+2] == 0 && data[pos+3] == 1) {
                startCodeLen = 4;
            }
        }
        
        if (startCodeLen == 0) {
            pos++;
            continue;
        }
        
        // 查找下一个start code
        size_t nextPos = findNextStartCode(data, size, pos + startCodeLen);
        size_t naluLen = nextPos - pos - startCodeLen;
        
        // 写入4字节长度(大端)
        avcc.push_back((naluLen >> 24) & 0xFF);
        avcc.push_back((naluLen >> 16) & 0xFF);
        avcc.push_back((naluLen >> 8) & 0xFF);
        avcc.push_back(naluLen & 0xFF);
        
        // 写入NALU数据
        avcc.insert(avcc.end(), 
                   data + pos + startCodeLen, 
                   data + pos + startCodeLen + naluLen);
        
        pos = nextPos;
    }
    
    return avcc;
}
```

### 3.3 IDR帧与GOP结构

#### 帧类型

- **IDR (Instantaneous Decoder Refresh)**: 即时解码刷新帧，完全独立解码，清空参考帧缓冲区
- **I帧**: Intra帧，帧内编码，但可能依赖前面的参考帧(非IDR的帧)
- **P帧**: Predicted帧，参考前面的I/P帧
- **B帧**: Bi-directional帧，参考前后的帧

**IDR vs I帧**:
- IDR是特殊的I帧(NALU type=5)
- 普通I帧(NALU type=1)，可能参考前面的帧
- **直播推流必须从IDR开始**，播放器才能解码

#### GOP (Group of Pictures)

一个IDR到下一个IDR之间的所有帧构成一个GOP。

**典型GOP结构** (无B帧，直播常用):
```
IDR P P P P IDR P ...
|<--  GOP = 30帧  -->|
```

**GOP长度对直播的影响**:
- **短GOP (1-2秒)**: 
  - 优点：快速恢复、换台快、丢帧影响小
  - 缺点：码率高、编码复杂度高
- **长GOP (4-10秒)**:
  - 优点：压缩率高、码率低
  - 缺点：丢帧恢复慢、首帧慢

**推荐配置**:
- 直播: GOP = 2秒 (60fps时120帧，30fps时60帧)
- 超低延迟: GOP = 1秒

#### GOP感知丢帧策略

你的SDK中的`bounded_queue.h`实现了GOP感知丢帧:

```cpp
// 伪代码
struct GOP {
    std::vector<MediaSample*> frames;
    int64_t startDts;
    int64_t endDts;
};

// 积压超阈值时
if (queueDelayMs > dropGopThresholdMs) {
    // 丢弃最旧的完整GOP
    if (!gopQueue.empty()) {
        GOP& oldestGop = gopQueue.front();
        for (auto* frame : oldestGop.frames) {
            if (!frame->sent) {
                dropFrame(frame);
            }
        gopQueue.pop_front();
        // 请求编码器产生新IDR
        onRequestKeyframe();
    }
}
```

**优势**:
- 保证解码连续性(不会丢半个GOP导致花屏)
- 请求新IDR快速恢复
- 优于盲目丢单帧

---

## 4. AAC音频编码格式

### 4.1 AAC基础

**AAC (Advanced Audio Coding)** 是MPEG-4标准的音频编码格式，相比MP3在同码率下音质更好。

#### AAC Profile

| Profile | 复杂度 | 应用场景 |
|------|----------|
| AAC-LC | Low | 最常用，直播推流标配 |
| AAC-Main | High | 高音质，编码慢 |
| AAC-HE | Medium | 低码率(32-64 kbps) |
| AAC-HEv2 | Medium | 超低码率(16-32 kbps) |

**直播推流推荐**: AAC-LC,44.1kHz, Stereo, 128kbps

### 4.2 ADTS vs Raw AAC

#### ADTS (Audio Data Transport Stream)

带同步头的AAC格式，每个帧独立解码:

```
[ADTS Header 7B][AAC Frame] [ADTS Header 7B][AAC Frame] ...
```

**ADTS Header结构** (7或9字节):
```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-
|1 1 1 1 1 1|1 1 1 1|0|  profile |freq|0|  ch |0|0|0| len |
+-+-+-+-+-+-+-+-+
|       length          |0 1 1 1 1 1|    ...          |
+-+-+-+-+-+-+-+-+
```

- **syncword**: 0xFFF (12 bits)
- **profile**: audioObjectType - 1
- **sampling_frequency_index**: 采样率索引
- **channel_configuration**: 声道配置
- **frame_length**: 包含头部的总长度

#### Raw AAC

不含同步头，只有纯AAC数据。

**RTMP要求Raw AAC**，需要去掉ADTS头:

```cpp
// 解析ADTS头获取帧长度
size_t parseAdtsFrameLength(const uint8_t* adts) {
    return ((adts[3] & 0x03) << 11) | 
           (adts[4] << 3) | 
           ((adts[5] & 0xE0) >> 5);
}

// 去除ADTS头
const uint8_t* adtsFrame = ...;
int adtsHeaderSize = ((adtsFrame[1] & 0x01) == 0) ? 9 : 7;  // 有无CRC
const uint8_t* rawAAC = adtsFrame + adtsHeaderSize;
size_t rawSize = parseAdtsFrameLength(adtsFrame) - adtsHeaderSize;
```

### 4.3 AudioSpecificConfig深入

**ASC编码位布局**:
```
audioObjectType (5 bits)
samplingFrequencyIndex (4 bits)
if (samplingFrequencyIndex == 0xF) {
    samplingFrequency (24 bits)
}
channelConfiguration (4 bits)
frameLengthFlag (1 bit)       // 0=1024 samples, 1=960 samples
dependsOnCoreCoder (1 bit)     // 通常为0
extensionFlag (1 bit)          // 通常为0
```

**完整计算示例**:

```cpp
// AAC-LC, 48kHz, 5.1声道
int audioObjectType = 2;       // AAC-LC
int samplingFreqIndex = 3;     // 48000 Hz
int channels = 6;              // 5.1声道

// Byte 1: [audioObjectType(5)] [samplingFreqIndex高3位(3)]
uint8_t byte1 = (audioObjectType << 3) | (samplingFreqIndex >> 1);
// = (2 << 3) | (3 >> 1) = 0x10 | 0x01 = 0x11

// Byte 2: [samplingFreqIndex低1位(1)] [channels(4)] [flags(3)]
uint8_t byte2 = ((samplingFreqIndex & 0x1) << 7) | (channels << 3);
// = (1 << 7) | (6 << 3) = 0x80 | 0x30 = 0xB0

// ASC = [0x11, 0xB0]
```

**从ASC反向解析**:
```cpp
void parseAudioSpecificConfig(const uint8_t* asc, size_t size) {
    if (size < 2) return;
    
    int audioObjectType = (asc[0] >> 3) & 0x1F;
    int samplingFreqIndex = ((asc[0] & 0x07) << 1) | ((asc[1] >> 7) & 0x01);
    int channels = (asc[1] >> 3) & 0x0F;
    
    printf("Profile: %d, SampleRate Index: %d, Channels: %d\n",
           audioObjectType, samplingFreqIndex, channels);
}
```

---

## 5. AMF0数据编码

### 5.1 AMF0概述

**AMF0 (Action Message Format 0)** 是Adobe的二进制序列化格式，用于RTMP命令和元数据的编码。

**特点**:
- 类型标记 + 数据
- 支持嵌套对象
- 大端字节序

### 5.2 AMF0数据类型详解

#### Number (0x00)

```
+--------+--------------------------------+
|  0x00  |  value (8 bytes, IEEE 754 double, big endian)
+--------+----------------------------------+
```

**示例** (编码数字1.0):
```
0x00 0x3F 0xF0 0x00 0x00 0x
```

#### Boolean (0x01)

```
+--------+--------+
|  0x01  | value  |
+--------+--------+
           1 byte (0x00=false, 0x01=true)
```

#### String (0x02)

```
+--------+--------+----------------+
|  0x02  |  length(2B, BE) |  UTF-8 string   |
+--------+--------+--------+------------------+
```

**示例** (编码字符串"live"):
```
0x02 0x00 0x04 0x6C 0x69 0x76 0x65
     |length=4| 'l'  'i'  'v'  'e'
```

#### Object (0x03)

```
+--------+
|  0x03  |
+--------+
  [key-value pairs]
  key: UTF-8 string (2 bytes length + string,无类型标记)
  value: AMF0 value (递归编码)
  ...
+--------+--------
|  0x00  |  0x00  |  0x09  |  Object End Marker
+--------+--------+--------+
```

**示例** (编码对象 {app: "live", code: 1}):
```
0x03                // Object marker
  0x00 0x03                   // key length = 3
  0x61 0x70              // "app"
  0x02 0x00 0x04              // String value, length=4
  0x6C 0x69 0x76 0x65         // "live"
  0x00 0x04                // key length = 4
  0x63 0x6F 0x640x65         // "code"
  0x00 0x3F 0xF0 ...          // Number value = 1.0
  
0x00 0x00 0x09                // Object end
```

#### Null (0x05)

```
+--------+
|  0x05  |
+--------+
```

#### ECMA Array (0x08)

类似Object，但前面有4字节的元素计数:

```
+--------+--------+--------+--------+
|  0x08  |     count (4 bytes, big endian)    |
+--------+--------+--------+
  [key-value pairs]
  ...
+--------+--------+--------+
|  0x00  |  0x09  |
+--------+--------+--------+
```

**注意**: count可以不准确，以0x00 0x09结束为准。

### 5.3 AMF0编码实现

你的SDK中`amf0.cpp`的实现:

```cpp
void Amf0Value::encode(ByteBuffer& out) const {
    switch (type_) {
    case Type::Number:
        out.writeU8(0x00);
        out.writeF64BE(number_);  // IEEE 754 double
        break;
        
    case Type::Boolean:
        out.writeU8(0x01);
        out.writeU8(bool_ ? 0x01 : 0x00);
        break;
        
    case Type::String:
        out.writeU8(0x02);
        out.writeU16BE(string_.size());
        out.write((const uint8_t*)string_.data(), string_.size());
        break;
        
    case Type::Object:
        out.writeU8(0x03);
        for (const auto& [key, value] : members_) {
            // key不带类型标记，只有长度和字符串
            out.writeU16BE(key.size());
            out.write((const uint8_t*)key.data(), key.size());
            // value递归编码
            value.encode(out);
        }
        // Object end marker
        out.writeU16BE(0);
        out.writeU8(0x09);
        break;
        
    case Type::Null:
        out.writeU8(0x05);
        break;
        
    case Type::EcmaArray:
        out.writeU8(0x08);
        out.writeU32BE(members_.size());  // count
        for (const auto& [key, value] : members_) {
            out.writeU16BE(key.size());
            out.write((const uint8_t*)key.data(), key.size());
            value.encode(out);
        }
        out.writeU16BE(0);
        out.writeU8(0x09);
        break;
    }
}
```

### 5.4 构造RTMP命令示例

**connect命令**:
```cpp
Amf0Value connectCmd;
connectCmd = Amf0Value::string("connect");  // 命令名

Amf0Value transactionId = Amf0Value::number(1.0);  // transaction ID

Amf0Value commandObj = Amf0Value::object();
commandObj.set("app", Amf0Value::string("live"));
commandObj.set("tcUrl", Amf0Value::string("rtmp://server:1935/live"));
commandObj.set("type", Amf0Value::string("nonprivate"));
commandObj.set("flashVer", Amf0Value::string("FMLE/3.0"));

// 编码
ByteBuffer buf;
connectCmd.encode(buf);
transactionId.encode(buf);
commandObj.encode(buf);

// buf现在包含完整的AMF0编码数据，作为Command Message的payload发送
```

---

## 6. 网络传输与非阻塞IO

### 6.1 为什么使用非阻塞IO

**直播推流的特殊需求**:
1. **编码线程不能阻塞**: 编码器以固定帧率产生数据(如30fps)，不能等待网络
2. **网络波动容忍**: 发送缓冲区满时不能卡死整个流程
3. **及时响应**: 需要及时处理服务器的ACK和控制消息

**阻塞IO的问题**:
```cpp
// 阻塞模式
int sent = send(socket, data, size, 0);
// 如果发送缓冲区满，会阻塞！
// 编码线程被卡住，无法产生新数据
```

**后果**:
- 编码线程阻塞→编码器输入队列满→视频采集丢帧
- 音频线程阻塞→音频采集丢帧→音画不同步
- 全局性能下降

### 6.2 非阻塞Socket编程

#### 设置非阻塞模式 (Windows)

```cpp
#include <winsock2.h>

SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

// 设置非阻塞
u_long mode = 1;
ioctlsocket(sock, FIONBIO, &mode);
```

#### 非阻塞connect

```cpp
int result = connect(sock, (sockaddr*)&addr, sizeof(addr));
if (result == SOCKET_ERROR) {
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
        // 连接正在进行中，需要等待可写事件
        return CONNECTING;
    } else {
        // 真正的错误
        return ERROR;
    }
}
return CONNECTED;
```

#### 非阻塞send

```cpp
int sent = send(sock, data, size, 0);
if (sent < 0) {
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
        // 发送缓冲区满，需要等待可写事件
        return 0;  // 本次发送0字节
    } else {
        // 连接断开或其他错误
        return -1;
    }
} else if (sent < size) {
    // 部分发送，剩余数据缓存起来下次发
    return sent;
}
return sent;
```

### 6.3 事件驱动模型

你的SDK使用`WSAPoll`实现事件循环:

```cpp
// tcp_transportselect.cpp
bool TcpTransport::poll(int timeoutMs, bool& readable, bool& writable) {
    WSAPOLLFD fds[1];
    fds[0].fd = socket_;
    fds[0].events = POLLIN;  // 关注可读
    if (hasPendingOutput_) {
        fds[0].events |= POLLOUT;  // 有数据待发送，关注可写
    }
    
    fds[0].revents = 0;
    int ret = WSAPoll(fds, 1, timeoutMs);
    if (ret > 0) {
        readable = (fds[0].revents & POLLIN) != 0;
        writable = (fds[0].revents & POLLOUT) != 0;
        return true;
    }
    
    return false;
}
```

### 6.4 发送缓冲区管理

**问题**: 非阻塞send可能部分发送，需要缓存未发送的数据。

**解决方案**: 维护一个发送缓冲区:

```cpp
class Session {
    ByteBuffer outBuffer_;  // 待发送数据
    void sendMediaMessage(...) {
        // 将消息编码到outBuffer_
        RtmpMessage msg = ...;
        chunkWriter_.write(msg, outBuffer_);
    }
    
    bool flush(size_t& written) {
        written = 0;
        while (outBuffer_.remaining() > 0) {
            const uint8_t* data = outBuffer_.readPtr();
            size_t size = outBuffer_.remaining();
            int sent = transport_->send(data, size);
            if (sent < 0) {
                return false;  // 连接错误
            } else if (sent == 0) {
                break;  // EWOULDBLOCK，下次再发
            }
            
            outBuffer_.skip(sent);
            written += sent;
            bytessentTotal_ += sent;
        }
        
        return true;
    }
};
```

### 6.5 完整的IO事件循环

你的SDK中`session.cpp`的`step()`方法:

```cpp
bool Session::step(int timeoutMs) {
    bool readable = false, writable = false;
    if (!transport_->poll(timeoutMs, readable, writable)) {
        return true;  // 超时，正常
    }
    
    // 处理可读事件
    if (readable) {
        if (!pumpRead()) {
            return false;  // 读取错误
        }
    }
    
    // 处理可写事件
    if (writable && out_.remaining() > 0) {
        size_t written = 0;
        if (!flush(written)) {
            return false;  // 发送错误
        }
        if (written > 0) {
            cb_.onBytesSent(written);
        }
    }
    
    return true;
}
```

**事件循环流程**:
```
loop:
  poll(timeout) → readable? writable?
  if readable:
    recv() → 增量解析握手/chunk/消息
    dispatch消息 → 调用onMessage处理
  if writable and 有数据待发:
    send() → 尽可能发送
    更新bytesSent统计
  回到loop
```

---

## 7. 时间戳与音视频同步

### 7.1 时间戳的三种表示

在推流链路中，时间戳有三种不同的表示:

| 时间基| 位置 | 用途 |
|--------|------|
| 微秒(μs) | 编码器输出 | AVPacket的pts/dts，你的MediaSample |
| 毫秒(ms) | RTMP协议 | Chunk Message Header的timestamp |
| 相对时间戳 | RTMP首帧 | 从0开始递增，便于服务器处理 |

### 7.2 PTS vs DTS

**DTS (Decode Timestamp)**: 解码时间戳，帧应该被解码器解码的时间

**PTS (Presentation Timestamp)**: 显示时间戳，帧应该被显示的时间

**无B帧时**: PTS = DTS

**有B帧时**: 显示顺序与解码顺序不同
```
编码顺序: I B P
解码顺序: I P B B (P帧需要先解码，B帧才能参考)
显示顺序: I B B P
```

**直播推流推荐**: 禁用B帧，简化时间戳处理

### 7.3 CompositionTime计算

FLV视频Tag中的**CompositionTime = PTS - DTS** (单位毫秒):

```cpp
// types.h中的MediaSample
struct MediaSample {
    int64_t dtsUs;  // 微秒
    int64_t ptsUs;  // 微秒
};

// flv_tag.cpp中的计算
int32_t compositionTimeMs = (sample.ptsUs - sample.dtsUs) / 1000;

// 转换为24位有符号整数
if (compositionTimeMs < 0) {
    compositionTimeMs = (1 << 24) + compositionTimeMs;  // 补码
}
```

### 7.4 RTMP时间戳转换

#### 微秒→毫秒

```cpp
// 编码器输出 (微秒)
int64_t dtsUs = sample.dtsUs;

// 转换为RTMP时间戳 (毫秒)
uint32_t rtmpTimestamp = dtsUs / 1000;
```

#### 相对时间戳

RTMP建议使用相对时间戳(从0开始):

```cpp
class Session {
    int64_t baseTimestampUs_ = -1;  // 首帧时间戳
    
    uint32_t toRtmpTimestamp(int64_t timestampUs) {
        if (baseTimestampUs_ < 0) {
            baseTimestampUs_ = timestampUs;
        }
        int64_t relativeUs = timestampUs - baseTimestampUs_;
        return relativeUs / 1000;  // 转毫秒
    }
};
```

**好处**:
- 时间戳从0开始，不会溢出24位上限(4.6小时)
- 重连时重置基准，服务器易于处理
- 符合RTMP规范建议

### 7.5 音视频时间戳对齐

**问题**: 音频和视频来自不同的编码器，时间基可能不同。

**解决方案**: 统一到同一个时间基(如系统时钟):

```cpp
// StreamController中的AVSyncClock
class AVSyncClock {
    std::chrono::steady_clock::time_point startTime_;
    
public:
    AVSyncClock() {
        reset();
    }
    
    int64_t now() {
        auto elapsed = std::chrono::steady_clock::now() - startTime_;
        return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    }
    
    void reset() {
        startTime_ = std::chrono::steady_clock::now();
    }
};

// 编码时打时间戳
AVSyncClock clock;
clock.reset();  // 推流开始时

// 视频编码完成
MediaSample videoSample;
videoSample.dtsUs = clock.now();
videoSample.ptsUs = videoSample.dtsUs;  // 无B帧

// 音频编码完成
MediaSample audioSample;
audioSample.dtsUs = clock.now();
audioSample.ptsUs = audioSample.dtsUs;
```

**注意事项**:
- 使用`steady_clock`而非`system_clock`，避免系统时间调整影响
- 音视频使用相同的clock实例
- 时间戳必须单调递增

### 7.6 时间戳异常处理

#### 时间戳回退检测

```cpp
class TimestampValidator {
    int64_t lastVideoTs_ = -1;
    int64_t lastAudioTs_ = -1;
    
public:
    bool validateVideo(int64_t ts) {
        if (lastVideoTs_ >= 0 && ts < lastVideoTs_) {
            // 时间戳回退
            LOG_WARN("Video timestamp rollback: {} -> {}", lastVideoTs_, ts);
            return false;
        }
        lastVideoTs_ = ts;
        return true;
    }
    
    bool validateAudio(int64_t ts) {
        if (lastAudioTs_ >= 0 && ts < lastAudioTs_) {
            LOG_WARN("Audio timestamp rollback: {} -> {}", lastAudioTs_, ts);
            return false;
        }
        lastAudioTs_ = ts;
        return true;
    }
};
```

#### 时间戳回绕处理

RTMP时间戳是32位无符号整数，最大值约49.7天:

```
0xFFFFFFFF ms = 4294967295 ms ≈ 49.7 天
```

**长时间推流时会回绕**，需要正确处理:

```cpp
class TimestampWrapper {
    uint32_t lastTs_ = 0;
    uint64_t epoch_ = 0;  // 回绕次数
    
public:
    uint32_t wrap(int64_t timestampUs) {
        uint32_t ts = (timestampUs / 1000) & 0xFFFFFFFF;
        
        // 检测回绕 (新时间戳比上次小很多)
        if (lastTs_ > 0xF0000000 && ts < 0x10000000) {
            epoch_++;
            LOG_INFO("Timestamp wrapped, epoch={}", epoch_);
        }
        
        lastTs_ = ts;
        return ts;
    }
    
    uint64_t getExtendedTimestamp() {
        return (epoch_ << 32) | lastTs_;
    }
};
```

### 7.7 音视频同步策略

#### 延迟测量

```cpp
struct AVDelay {
    int64_t videoLatestDts = -1;
    int64_t audioLatestDts = -1;
    
    int64_t getAVGap() const {
        if (videoLatestDts < 0 || audioLatestDts < 0) return 0;
        return videoLatestDts - audioLatestDts;  // 正值=视频快，负值=音频快
    }
};

// 在推送时更新
void onPushVideo(const MediaSample& sample) {
    avDelay_.videoLatestDts = sample.dtsUs;
    checkAVSync();
}

void onPushAudio(const MediaSample& sample) {
    avDelay_.audioLatestDts = sample.dtsUs;
    checkAVSync();
}
```

#### 音视频失步处理

```cpp
void checkAVSync() {
    int64_t gapUs = avDelay_.getAVGap();
    int64_t gapMs = gapUs / 1000;
    
    if (std::abs(gapMs) > 200) {
        LOG_WARN("AV sync issue: gap={}ms (video {} audio)", 
                 gapMs, gapMs > 0 ? "ahead" : "behind");
        
        // 策略1: 等待慢的一路
        if (gapMs > 500) {
            // 视频太快，暂停接收新视频帧
            pauseVideoEnqueue_ = true;
        } else if (gapMs < -500) {
            // 音频太快，暂停接收新音频帧
            pauseAudioEnqueue_ = true;
        }
        
        // 策略2: 通知上层调整编码器
        if (std::abs(gapMs) > 1000) {
            cb_.onAVDesync(gapMs);
        }
    } else {
        // 恢复正常
        pauseVideoEnqueue_ = false;
        pauseAudioEnqueue_ = false;
    }
}
```

**同步原则**:
- **容忍范围**: ±200ms内不处理
- **轻微失步** (200-500ms): 记录警告
- **严重失步** (>500ms): 暂停快的一路
- **极端失步** (>1s): 通知上层重置编码器

### 7.8 首帧对齐策略

**问题**: 推流开始时，音频和视频哪个先发？

**策略**: 等待首个视频关键帧后，再开始发送音视频

```cpp
class FirstFrameSync {
    bool hasVideoKey_ = false;
    bool hasAudio_ = false;
    std::vector<MediaSample> audioBuffer_;  // 缓存音频
    
public:
    bool shouldSend(const MediaSample& sample) {
        if (sample.type == SampleType::VideoKey) {
            hasVideoKey_ = true;
            // 视频关键帧到达，释放缓存的音频
            flushAudioBuffer();
            return true;
        }
        
        if (sample.type == SampleType::AudioRaw) {
            hasAudio_ = true;
            if (!hasVideoKey_) {
                // 视频关键帧还未到，缓存音频
                audioBuffer_.push_back(sample);
                return false;
            }
            return true;
        }
        
        // 非关键帧视频
        return hasVideoKey_;  // 只有关键帧到过才能发
    }
};
```

**好处**:
- 保证播放器首帧能解码(从IDR开始)
- 音画从头就是同步的
- 避免首秒黑屏或花屏

---

## 8. 状态机与会话管理

### 8.1 RTMP会话状态机

你的SDK中定义了详细的会话状态(`types.h`):

```cpp
enum class SessionState {
    Idle,            // 未启动
    Resolving,       // DNS解析中
    Connecting,      // TCP连接中
    Handshaking,     // RTMP握手中
    AppConnecting,   // connect命令中
    StreamCreating,  // createStream命令中
    Publishing,      // publish命令中
    Streaming,       // 正常推流中
    Backoff,         // 退避等待重连
    Reconnecting,    // 重连中
    Stopped,         // 已停止
    Error            // 错误状态
};
```

### 8.2 状态转换流程图

```
         ┌──────┐
    ┌───>│ Idle │
    │    └──┬───┘
    │       │ start()
    │       ▼
    │  ┌──────────┐
    │  │Resolving │ DNS解析
    │  └────┬─────┘
    │       │ resolved
    │       ▼
    │  ┌───────────┐
    │  │Connecting │ TCP连接
    │  └─────┬─────┘
    │        │ connected
    │        ▼
    │  ┌────────────┐
    │  │Handshaking │ C0+C1+C2
    │  └──────┬─────┘
    │         │ S2 received
    │         ▼
    │  ┌──────────────┐
    │  │AppConnecting │ connect命令
    │  └───────┬──────┘
    │          │ _result received
    │          ▼
    │  ┌───────────────┐
    │  │StreamCreating │ createStream命令
    │  └────────┬──────┘
    │           │ stream ID received
    │           ▼
    │  ┌───────────┐
    │  │Publishing │ publish命令
    │  └─────┬─────┘
    │        │ onStatus(publish start)
    │        ▼
    │  ┌──────────┐
    │  │Streaming │◄────┐ 正常推流
    │  └────┬─────┘     │
    │       │           │
    │       │ error     │ 持续发送音视频
    │       ▼           │
    │  ┌───────┐        │
    │  │ Error │        │
    │  └───┬───┘        │
    │      │            │
    │      │ auto retry │
    │      ▼            │
    │  ┌─────────┐      │
    └──│ Backoff │      │
       └────┬────┘      │
            │ delay     │
            ▼           │
       ┌────────────┐   │
       │Reconnecting│───┘
       └────────────┘
```

### 8.3 状态机实现

#### 基础状态管理

```cpp
class Session {
    SessionState state_ = SessionState::Idle;
    StateCallback stateCb_;
    
public:
    void setState(SessionState newState, const std::string& detail = "") {
        if (state_ == newState) return;
        
        LOG_INFO("State: {} -> {} ({})", 
                 toString(state_), toString(newState), detail);
        
        state_ = newState;
        
        if (stateCb_) {
            stateCb_(newState, detail);
        }
    }
    
    SessionState getState() const { return state_; }
    
    bool isStreaming() const { 
        return state_ == SessionState::Streaming; 
    }
    
    bool isConnected() const {
        return state_ >= SessionState::Handshaking && 
               state_ <= SessionState::Streaming;
    }
};
```

#### 状态驱动的消息处理

```cpp
void Session::onMessageReceived(const RtmpMessage& msg) {
    switch (state_) {
    case SessionState::Handshaking:
        if (msg.typeId == MessageType::Handshake) {
            handleHandshakeResponse(msg);
        }
        break;
        
    case SessionState::AppConnecting:
        if (msg.typeId == MessageType::Command) {
            handleConnectResponse(msg);
        }
        break;
        
    case SessionState::StreamCreating:
        if (msg.typeId == MessageType::Command) {
            handleCreateStreamResponse(msg);
        }
        break;
        
    case SessionState::Publishing:
        if (msg.typeId == MessageType::Command) {
            handlePublishResponse(msg);
        }
        break;
        
    case SessionState::Streaming:
        handleStreamingMessage(msg);
        break;
        
    default:
        LOG_WARN("Unexpected message in state {}", toString(state_));
        break;
    }
}
```

### 8.4 连接流程状态转换

```cpp
bool Session::start() {
    if (state_ != SessionState::Idle) {
        LOG_ERROR("Cannot start from state {}", toString(state_));
        return false;
    }
    
    setState(SessionState::Resolving, "DNS lookup");
    
    // 异步DNS解析
    resolveAsync(url_.host, [this](bool success, const std::string& ip) {
        if (!success) {
            setState(SessionState::Error, "DNS resolve failed");
            scheduleReconnect();
            return;
        }
        
        remoteIp_ = ip;
        connectTcp();
    });
    
    return true;
}

void Session::connectTcp() {
    setState(SessionState::Connecting, "TCP connecting to " + remoteIp_);
    
    bool success = transport_->connect(remoteIp_, url_.port);
    if (!success) {
        setState(SessionState::Error, "TCP connect failed");
        scheduleReconnect();
        return;
    }
    
    // 非阻塞连接，等待可写事件
    setState(SessionState::Connecting, "TCP connect in progress");
}

void Session::onConnected() {
    LOG_INFO("TCP connected");
    setState(SessionState::Handshaking, "RTMP handshake");
    
    // 发送C0+C1
    handshake_.start(outBuffer_);
}

void Session::onHandshakeComplete() {
    LOG_INFO("Handshake complete");
    setState(SessionState::AppConnecting, "Sending connect");
    
    sendConnect();
}

void Session::onConnectResult(bool success) {
    if (!success) {
        setState(SessionState::Error, "connect command rejected");
        scheduleReconnect();
        return;
    }
    
    LOG_INFO("App connected");
    setState(SessionState::StreamCreating, "Creating stream");
    
    sendCreateStream();
}

void Session::onStreamCreated(uint32_t streamId) {
    streamId_ = streamId;
    LOG_INFO("Stream created: ID={}", streamId);
    setState(SessionState::Publishing, "Publishing stream");
    
    sendPublish();
}

void Session::onPublishStart() {
    LOG_INFO("Publish started");
    setState(SessionState::Streaming, "Ready to stream");
    
    // 发送元数据
    sendMetadata();
    
    // 发送视频/音频配置帧
    if (hasVideoParams_) {
        sendVideoSequenceHeader();
    }
    if (hasAudioParams_) {
        sendAudioSequenceHeader();
    }
    
    // 通知上层可以开始推流
    if (cb_.onReadyToStream) {
        cb_.onReadyToStream();
    }
}
```

### 8.5 Transaction ID管理

RTMP命令使用transaction ID匹配请求和响应:

```cpp
class TransactionManager {
    uint32_t nextId_ = 1;
    std::unordered_map<uint32_t, std::function<void(bool, const Amf0Value&)>> pending_;
    
public:
    uint32_t allocate(std::function<void(bool, const Amf0Value&)> callback) {
        uint32_t id = nextId_++;
        pending_[id] = callback;
        return id;
    }
    
    void handleResponse(uint32_t id, bool success, const Amf0Value& result) {
        auto it = pending_.find(id);
        if (it == pending_.end()) {
            LOG_WARN("Unknown transaction ID: {}", id);
            return;
        }
        
        it->second(success, result);
        pending_.erase(it);
    }
    
    void clear() {
        pending_.clear();
        nextId_ = 1;
    }
};
```

**使用示例**:

```cpp
void Session::sendConnect() {
    uint32_t txId = txMgr_.allocate([this](bool success, const Amf0Value& result) {
        onConnectResult(success);
    });
    
    // 构造connect命令
    RtmpMessage msg;
    msg.typeId = MessageType::Command;
    msg.csid = 3;
    msg.streamId = 0;
    
    ByteBuffer payload;
    Amf0Value::string("connect").encode(payload);
    Amf0Value::number(txId).encode(payload);
    // ... 编码命令对象
    
    sendMessage(msg);
}

void Session::onCommandMessage(const RtmpMessage& msg) {
    // 解析AMF0
    Amf0Decoder decoder(msg.payload);
    std::string command = decoder.decodeString();
    double txId = decoder.decodeNumber();
    
    if (command == "_result") {
        Amf0Value result = decoder.decode();
        txMgr_.handleResponse(txId, true, result);
    } else if (command == "_error") {
        Amf0Value error = decoder.decode();
        txMgr_.handleResponse(txId, false, error);
    }
}
```

### 8.6 连接超时控制

```cpp
class TimeoutManager {
    std::chrono::steady_clock::time_point stateEnterTime_;
    int timeoutMs_;
    
public:
    void enter(int timeoutMs) {
        stateEnterTime_ = std::chrono::steady_clock::now();
        timeoutMs_ = timeoutMs;
    }
    
    bool isTimeout() const {
        auto elapsed = std::chrono::steady_clock::now() - stateEnterTime_;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        return elapsedMs > timeoutMs_;
    }
    
    int remaining() const {
        auto elapsed = std::chrono::steady_clock::now() - stateEnterTime_;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        return std::max(0, timeoutMs_ - static_cast<int>(elapsedMs));
    }
};

// 在状态转换时启动超时
void Session::setState(SessionState newState, const std::string& detail) {
    // ... 更新状态
    
    // 设置超时
    switch (newState) {
    case SessionState::Connecting:
    case SessionState::Handshaking:
    case SessionState::AppConnecting:
    case SessionState::StreamCreating:
    case SessionState::Publishing:
        timeout_.enter(config_.connectTimeoutMs);
        break;
    default:
        break;
    }
}

// 在事件循环中检查超时
bool Session::step(int timeoutMs) {
    // 检查状态超时
    if (timeout_.isTimeout()) {
        LOG_ERROR("Operation timeout in state {}", toString(state_));
        setState(SessionState::Error, "Timeout");
        scheduleReconnect();
        return false;
    }
    
    // ... IO事件处理
}
```

### 8.7 会话生命周期管理

#### 资源初始化

```cpp
bool Session::initialize() {
    // 解析URL
    if (!url_.parse(config_.url)) {
        LOG_ERROR("Invalid URL: {}", config_.url);
        return false;
    }
    
    // 创建传输层
    transport_ = std::make_unique<TcpTransport>();
    
    // 初始化握手
    handshake_ = std::make_unique<Handshake>();
    
    // 初始化Chunk读写器
    chunkReader_ = std::make_unique<ChunkReader>();
    chunkWriter_ = std::make_unique<ChunkWriter>();
    chunkWriter_->setChunkSize(config_.chunkSize);
    
    // 初始化缓冲区
    inBuffer_.reserve(65536);
    outBuffer_.reserve(65536);
    
    return true;
}
```

#### 资源清理

```cpp
void Session::cleanup() {
    // 关闭传输层
    if (transport_) {
        transport_->close();
    }
    
    // 清理事务
    txMgr_.clear();
    
    // 清理缓冲区
    inBuffer_.clear();
    outBuffer_.clear();
    
    // 重置状态
    streamId_ = 0;
    baseTimestampUs_ = -1;
    hasVideoParams_ = false;
    hasAudioParams_ = false;
    
    setState(SessionState::Idle);
}
```

#### 重连时的状态保留

```cpp
struct SessionContext {
    VideoParams videoParams;
    AudioParams audioParams;
    bool hasVideoParams = false;
    bool hasAudioParams = false;
};

void Session::reconnect() {
    // 保存上下文
    SessionContext ctx;
    ctx.videoParams = videoParams_;
    ctx.audioParams = audioParams_;
    ctx.hasVideoParams = hasVideoParams_;
    ctx.hasAudioParams = hasAudioParams_;
    
    // 清理旧连接
    cleanup();
    
    // 恢复上下文
    videoParams_ = ctx.videoParams;
    audioParams_ = ctx.audioParams;
    hasVideoParams_ = ctx.hasVideoParams;
    hasAudioParams_ = ctx.hasAudioParams;
    
    // 重新连接
    start();
}
```

### 8.8 多线程安全

你的SDK使用独立的IO线程:

```cpp
class PublisherImpl {
    std::thread ioThread_;
    std::atomic<bool> running_;
    
    // 线程安全的队列
    BoundedQueue videoQueue_;
    BoundedQueue audioQueue_;
    
public:
    bool start() {
        running_ = true;
        ioThread_ = std::thread([this]() {
            ioThreadMain();
        });
        return true;
    }
    
    void stop() {
        running_ = false;
        if (ioThread_.joinable()) {
            ioThread_.join();
        }
    }
    
    // 从编码线程调用
    bool pushVideo(const MediaSample& sample) {
        if (!running_) return false;
        return videoQueue_.push(sample);  // 线程安全
    }
    
    // IO线程主循环
    void ioThreadMain() {
        while (running_) {
            // 从队列取数据
            MediaSample sample;
            if (videoQueue_.tryPop(sample, 10)) {
                session_->sendVideoSample(sample);
            }
            if (audioQueue_.tryPop(sample, 10)) {
                session_->sendAudioSample(sample);
            }
            
            // 处理网络IO
            session_->step(10);
        }
    }
};
```

**线程模型**:
```
编码线程 ──► [线程安全队列] ──► IO线程 ──► 网络
(pushVideo)                   (session.step)
(pushAudio)
```

**好处**:
- 编码线程无阻塞，快速返回
- IO线程专注网络通信
- 队列解耦，提高并发性

---

## 9. 背压控制与流控

### 9.1 背压问题分析

**背压 (Backpressure)** 是指下游处理速度跟不上上游生产速度，导致数据积压。

#### 推流场景的背压源

```
编码器 ──► 推流队列 ──► 网络发送 ──► 服务器
 (固定)      (缓冲)      (可变)      (处理)
 30fps                  受带宽限制
```

**问题**:
1. **网络慢**: 上行带宽不足，发送缓冲区满
2. **服务器慢**: 服务器处理慢，TCP窗口收缩
3. **编码快**: 编码器固定30fps产生数据，不能停

**后果**:
- 队列持续增长 → 内存占用增加
- 延迟累积 → 直播延迟从3秒增长到10秒甚至更高
- OOM风险 → 长时间积压可能耗尽内存

### 9.2 队列延迟测量

```cpp
class BoundedQueue {
    struct QueuedSample {
        MediaSample sample;
        int64_t enqueueTimeUs;  // 入队时间
    };
    
    std::deque<QueuedSample> queue_;
    
public:
    int64_t getDelayUs() const {
        if (queue_.empty()) return 0;
        
        // 队首元素的等待时间
        int64_t now = getCurrentTimeUs();
        return now - queue_.front().enqueueTimeUs;
    }
    
    int64_t getMediaDelayUs() const {
        if (queue_.empty()) return 0;
        
        // 队首到队尾的媒体时间跨度
        int64_t headDts = queue_.front().sample.dtsUs;
        int64_t tailDts = queue_.back().sample.dtsUs;
        return tailDts - headDts;
    }
};
```

**两种延迟**:
- **墙上时钟延迟**: 数据在队列中等待的真实时间
- **媒体时间延迟**: 队列中数据的时间戳跨度(更准确反映直播延迟)
 
 
 
 # # #   9 . 3   R��U0wÕ�q�W˓5�/p
 
 
 
 cm�r�kS D K cm�deR��U��(��RvqYt�Uv�Õ�q�W( ` b o u n d e d _ q u e u e . h ` ) : 
 
 
 
 ` ` ` c p p 
 
 c l a s s   D u a l Q u e u e   { 
 
         B o u n d e d Q u e u e   v i d e o Q u e u e _ ; 
 
         B o u n d e d Q u e u e   a u d i o Q u e u e _ ; 
 
         P u b l i s h e r C o n f i g   c o n f i g _ ; 
 
 
 
 p u b l i c : 
 
         b o o l   p u s h V i d e o ( c o n s t   M e d i a S a m p l e &   s a m p l e )   { 
 
                 i n t 6 4 _ t   d e l a y M s   =   v i d e o Q u e u e _ . g e t M e d i a D e l a y U s ( )   /   1 0 0 0 ; 
 
                 i f   ( d e l a y M s   >   c o n f i g _ . m a x Q u e u e D e l a y M s )   { 
 
                         i f   ( d r o p O l d e s t G o p ( ) )   { 
 
                                 s t a t s _ . g o p D r o p p e d + + ; 
 
                                 r e q u e s t K e y f r a m e ( ) ; 
 
                         } 
 
                 } 
 
                 r e t u r n   v i d e o Q u e u e _ . p u s h ( s a m p l e ) ; 
 
         } 
 
 
 
         b o o l   p u s h A u d i o ( c o n s t   M e d i a S a m p l e &   s a m p l e )   { 
 
                 i n t 6 4 _ t   d e l a y M s   =   a u d i o Q u e u e _ . g e t M e d i a D e l a y U s ( )   /   1 0 0 0 ; 
 
                 i f   ( d e l a y M s   >   c o n f i g _ . m a x A u d i o Q u e u e M s )   { 
 
                         a u d i o Q u e u e _ . p o p F r o n t ( ) ; 
 
                 } 
 
                 r e t u r n   a u d i o Q u e u e _ . p u s h ( s a m p l e ) ; 
 
         } 
 
 } ; 
 
 ` ` ` 
 
 
 
 * * �tPcxᰓ�_Xm* * : 
 
 -   ʕ�bv�\��\K��hb�W�[� �~��`�|\0|�m�]qQ��? 
 
 -   Yt�Uv�Y��N�m!2�f�m�rO P ��\vq�hb�mW��f�b
 
 -   ���W�SYt�Uv��~�^0�^S7dʕ�bv�9p�pi��? 
 
 
 
 # # #   9 . 4   G O P ���qaq�m 2�b�~+h�f
 
 
 
 # # # #   �mp��|�m#X�渓FXO P �m 2�b
 
 
 
 )�X[4^�m 2]/uB0}5pp��VYtG�rc�%1,_p��Q�b�TQ0�|\��"��q�Y^p�_� �P�~���N	Z�f�i�m 2}9p~\�f(��QO P �? 
 
 
 
 ` ` ` 
 
 �m 2]/u? ( ��k��) : 
 
 I D R   P   P   [ �m6�]   P   P   I D R   . . . 
 
                             +�? Z��^;uP /uC,_p��Q!|t��m(��R�b  +�? z�^SFw
 
 
 
 �m!2�f�m�rO P   ( �YG� ) : 
 
 [ I D R   P   P   P   P ]   I D R   P   P   P   . . . 
 
   +�? ���ܑG O P �m 2}    +�? `m�^�gI D R �[� �o6[}YtG�r�YE�6r
 
 ` ` ` 
 
 
 
 # # # #   G O P Hg-Wkf�Y� 4Z*[�u�? 
 
 
 
 ` ` ` c p p 
 
 s t r u c t   G O P   { 
 
         s t d : : v e c t o r < M e d i a S a m p l e >   f r a m e s ; 
 
         i n t 6 4 _ t   s t a r t D t s   =   0 ; 
 
         i n t 6 4 _ t   e n d D t s   =   0 ; 
 
         b o o l   c o m p l e t e   =   f a l s e ; 
 
 } ; 
 
 
 
 c l a s s   G o p Q u e u e   { 
 
         s t d : : d e q u e < G O P >   g o p s _ ; 
 
         G O P   c u r r e n t G o p _ ; 
 
 
 
 p u b l i c : 
 
         v o i d   p u s h ( c o n s t   M e d i a S a m p l e &   s a m p l e )   { 
 
                 i f   ( s a m p l e . t y p e   = =   S a m p l e T y p e : : V i d e o K e y )   { 
 
                         i f   ( ! c u r r e n t G o p _ . f r a m e s . e m p t y ( ) )   { 
 
                                 c u r r e n t G o p _ . c o m p l e t e   =   t r u e ; 
 
                                 c u r r e n t G o p _ . e n d D t s   =   c u r r e n t G o p _ . f r a m e s . b a c k ( ) . d t s U s ; 
 
                                 g o p s _ . p u s h _ b a c k ( s t d : : m o v e ( c u r r e n t G o p _ ) ) ; 
 
                         } 
 
                         c u r r e n t G o p _   =   G O P ( ) ; 
 
                         c u r r e n t G o p _ . s t a r t D t s   =   s a m p l e . d t s U s ; 
 
                 } 
 
                 c u r r e n t G o p _ . f r a m e s . p u s h _ b a c k ( s a m p l e ) ; 
 
         } 
 
 
 
         b o o l   d r o p O l d e s t G o p ( )   { 
 
                 i f   ( g o p s _ . e m p t y ( ) )   r e t u r n   f a l s e ; 
 
                 G O P &   o l d e s t   =   g o p s _ . f r o n t ( ) ; 
 
                 L O G _ W A R N ( " D r o p p i n g   G O P :   { }   f r a m e s ,   { } m s " , 
 
                                   o l d e s t . f r a m e s . s i z e ( ) , 
 
                                   ( o l d e s t . e n d D t s   -   o l d e s t . s t a r t D t s )   /   1 0 0 0 ) ; 
 
                 g o p s _ . p o p _ f r o n t ( ) ; 
 
                 r e t u r n   t r u e ; 
 
         } 
 
 } ; 
 
 ` ` ` 
 
 
 
 # # # #   �mY0ÕX� ���t#�? 
 
 
 
 5ppt2|cm�r�kS D K ���]�u( ` P u b l i s h e r C o n f i g ` ) : 
 
 
 
 ` ` ` 
 
 m a x Q u e u e D e l a y M s         =   8 0 0 m s       +�? �m^�[�~�^��\�j.�zO�]
 
 d r o p G o p T h r e s h o l d M s   =   1 2 0 0 m s     +�? �m�0xV�~�^�|\�mG O P 
 
 ` ` ` 
 
 
 
 ` ` ` c p p 
 
 v o i d   h a n d l e B a c k p r e s s u r e ( )   { 
 
         i n t 6 4 _ t   d e l a y M s   =   g o p Q u e u e _ . g e t T o t a l D e l a y M s ( ) ; 
 
 
 
         i f   ( d e l a y M s   >   c o n f i g _ . d r o p G o p T h r e s h o l d M s )   { 
 
                 i f   ( g o p Q u e u e _ . d r o p O l d e s t G o p ( ) )   { 
 
                         s t a t s _ . g o p D r o p p e d + + ; 
 
                         r e q u e s t K e y f r a m e ( ) ; 
 
                 } 
 
         }   e l s e   i f   ( d e l a y M s   >   c o n f i g _ . m a x Q u e u e D e l a y M s )   { 
 
                 i n t   t a r g e t B p s   =   c u r r e n t B p s _   *   0 . 8 ; 
 
                 i f   ( b i t r a t e C a l l b a c k _ )   b i t r a t e C a l l b a c k _ ( t a r g e t B p s ) ; 
 
         }   e l s e   i f   ( d e l a y M s   <   c o n f i g _ . m a x Q u e u e D e l a y M s   /   2 )   { 
 
                 i n t   t a r g e t B p s   =   s t d : : m i n ( ( i n t ) ( c u r r e n t B p s _   *   1 . 1 ) ,   m a x B p s _ ) ; 
 
                 i f   ( b i t r a t e C a l l b a c k _ )   b i t r a t e C a l l b a c k _ ( t a r g e t B p s ) ; 
 
         } 
 
 } 
 
 ` ` ` 
 
 
 
 # # #   9 . 5   O��bme/uF���Y�P�nR�? 
 
 
 
 cm�r�kS D K ��3lC~e��p�v��1laq+hrc�$1��"��q�S����b: 
 
 
 
 ` ` ` c p p 
 
 p u b l i s h e r . o n R e q u e s t K e y f r a m e ( [ & e n c o d e r ] ( )   { 
 
         e n c o d e r . f o r c e K e y f r a m e ( ) ; 
 
 } ) ; 
 
 
 
 / /   C ���0[_
 
 r t m p _ o n _ r e q u e s t _ k e y f r a m e ( p ,   [ ] ( v o i d *   u s e r )   { 
 
         s t a t i c _ c a s t < E n c o d e r * > ( u s e r ) - > f o r c e K e y f r a m e ( ) ; 
 
 } ,   & e n c o d e r ) ; 
 
 ` ` ` 
 
 
 
 # # # #   �t�0w�h b�]ĕ,a�W
 
 
 
 ` ` ` c p p 
 
 c l a s s   K e y f r a m e R e q u e s t L i m i t e r   { 
 
         s t d : : c h r o n o : : s t e a d y _ c l o c k : : t i m e _ p o i n t   l a s t R e q u e s t T i m e _ ; 
 
         i n t   m i n I n t e r v a l M s _   =   2 0 0 0 ; 
 
 
 
 p u b l i c : 
 
         b o o l   t r y R e q u e s t ( )   { 
 
                 a u t o   n o w   =   s t d : : c h r o n o : : s t e a d y _ c l o c k : : n o w ( ) ; 
 
                 a u t o   e l a p s e d   =   s t d : : c h r o n o : : d u r a t i o n _ c a s t < s t d : : c h r o n o : : m i l l i s e c o n d s > ( 
 
                         n o w   -   l a s t R e q u e s t T i m e _ ) . c o u n t ( ) ; 
 
                 i f   ( e l a p s e d   <   m i n I n t e r v a l M s _ )   r e t u r n   f a l s e ; 
 
                 l a s t R e q u e s t T i m e _   =   n o w ; 
 
                 r e t u r n   t r u e ; 
 
         } 
 
 } ; 
 
 ` ` ` 
 
 
 
 # # #   9 . 6   T�&1� zOr��V�V���P2|
 
 
 
 cm�r�kS D K �m�te w m a . h ` 9p�pG^���V�fT��rHo�~��Y���QNo: 
 
 
 
 ` ` ` c p p 
 
 t e m p l a t e < t y p e n a m e   T > 
 
 c l a s s   E W M A   { 
 
         T   v a l u e _ ; 
 
         d o u b l e   a l p h a _ ; 
 
         b o o l   i n i t i a l i z e d _   =   f a l s e ; 
 
 
 
 p u b l i c : 
 
         e x p l i c i t   E W M A ( d o u b l e   a l p h a   =   0 . 1 )   :   a l p h a _ ( a l p h a ) ,   v a l u e _ ( T { } )   { } 
 
 
 
         v o i d   u p d a t e ( T   n e w V a l u e )   { 
 
                 i f   ( ! i n i t i a l i z e d _ )   { 
 
                         v a l u e _   =   n e w V a l u e ; 
 
                         i n i t i a l i z e d _   =   t r u e ; 
 
                 }   e l s e   { 
 
                         v a l u e _   =   a l p h a _   *   n e w V a l u e   +   ( 1 . 0   -   a l p h a _ )   *   v a l u e _ ; 
 
                 } 
 
         } 
 
 
 
         T   g e t ( )   c o n s t   {   r e t u r n   v a l u e _ ;   } 
 
 } ; 
 
 ` ` ` 
 
 
 
 * * a l p h a Y��P�f��Y�Z* * :   0 . 1 ( ��$2� �q��JZ? ,   0 . 3 ( ��(1]) ,   0 . 5 ( G��� �q7d4d? 
 
 
 
 # # #   9 . 7   R T M P   A c k n o w l e d g e m e n t 
 
 
 
 9p!2�W�~�~uȓ�qB_��NC K [�DZaqȓ�]�Yc�%1�Q�����W(��R�tz��P�f��? 
 
 
 
 - - - 
 
 
 
 # #   1 0 .   ��k��o�R�`�m�^xVig�p�t#�? 
 
 
 
 # # #   1 0 . 1   ��k��R��U��
 
 
 
 # # # #   Y���N�o�]Je�t? 
 
 -    b�|,���g:   �~*[F]���][~
 
 -   D N S YtF�=p�o���:   ��� ��WWxVig?   
 
 -   ��!�X�tnTi:   ��� ��WWxVig? 
 
 
 
 # # # #   �m�]r_�� 22濕k��
 
 -   �t�0	v�o���:   K��nẓ&1fy
 
 -   U R L ͓Nq!}��k��:   K��nẓ&1fy
 
 -   4ZxO�`P�,�Js:   K��nẓ&1fy
 
 
 
 # # #   1 0 . 2   ��k��Y� 4Z? 
 
 
 
 ` ` ` c p p 
 
 b o o l   S e s s i o n : : p u m p R e a d ( )   { 
 
         i n t   r e c e i v e d   =   t r a n s p o r t _ - > r e c v ( b u f f e r ,   s i z e ) ; 
 
         
 
         i f   ( r e c e i v e d   <   0 )   { 
 
                 i f   ( e r r   ! =   E W O U L D B L O C K )   { 
 
                         L O G _ E R R O R ( " N e t w o r k   e r r o r " ) ; 
 
                         s c h e d u l e R e c o n n e c t ( ) ; 
 
                         r e t u r n   f a l s e ; 
 
                 } 
 
         }   e l s e   i f   ( r e c e i v e d   = =   0 )   { 
 
                 L O G _ I N F O ( " C o n n e c t i o n   c l o s e d " ) ; 
 
                 s c h e d u l e R e c o n n e c t ( ) ; 
 
                 r e t u r n   f a l s e ; 
 
         } 
 
         r e t u r n   t r u e ; 
 
 } 
 
 ` ` ` 
 
 
 
 # # #   1 0 . 3   ���V�f��� ��WWxVig? 
 
 
 
 ` ` ` c p p 
 
 c l a s s   E x p o n e n t i a l B a c k o f f   { 
 
         i n t   a t t e m p t _   =   0 ; 
 
         
 
 p u b l i c : 
 
         i n t   n e x t D e l a y ( )   { 
 
                 i n t   d e l a y   =   b a s e M s _   *   ( 1   < <   a t t e m p t _ ) ; 
 
                 d e l a y   =   s t d : : m i n ( d e l a y ,   m a x M s _ ) ; 
 
                 / /   #Z��Y��'h�Y  ( dS2 5 % ) 
 
                 r e t u r n   a d d J i t t e r ( d e l a y ) ; 
 
         } 
 
 } ; 
 
 ` ` ` 
 
 
 
 * * ��� ���Z0n? * :   1 s   +�? 2 s   +�? 4 s   +�? 8 s   +�? 1 6 s   +�? 3 0 s ( �mGZ�j) 
 
 
 
 # # #   1 0 . 4   ���][~4ZzO�%
 
 
 
 ` ` ` c p p 
 
 v o i d   S e s s i o n : : s c h e d u l e R e c o n n e c t ( )   { 
 
         c l e a n u p ( ) ; 
 
         s e t S t a t e ( S e s s i o n S t a t e : : B a c k o f f ) ; 
 
         r e c o n n e c t S c h e d u l e r _ . s c h e d u l e ( ) ; 
 
 } 
 
 
 
 v o i d   S e s s i o n : : o n C o n n e c t e d ( )   { 
 
         r e c o n n e c t S c h e d u l e r _ . r e s e t ( ) ;     / /   ���]�u��� ���x���g�j
 
 } 
 
 ` ` ` 
 
 
 
 # # #   1 0 . 5   O��bme/uE�t�[? 
 
 
 
 ���][~��,a�YZ��^}�~
Y�}O��bme/uC@UY�&b� }OK��h? 
 
 
 
 ` ` ` c p p 
 
 v o i d   S e s s i o n : : o n P u b l i s h S t a r t ( )   { 
 
         s e n d M e t a d a t a ( ) ; 
 
         s e n d V i d e o S e q u e n c e H e a d e r ( ) ; 
 
         s e n d A u d i o S e q u e n c e H e a d e r ( ) ; 
 
         
 
         w a i t i n g F o r K e y f r a m e _   =   t r u e ; 
 
         r e q u e s t K e y f r a m e ( ) ; 
 
 } 
 
 
 
 b o o l   S e s s i o n : : s e n d V i d e o S a m p l e ( c o n s t   M e d i a S a m p l e &   s a m p l e )   { 
 
         i f   ( w a i t i n g F o r K e y f r a m e _ )   { 
 
                 i f   ( s a m p l e . t y p e   = =   S a m p l e T y p e : : V i d e o K e y )   { 
 
                         w a i t i n g F o r K e y f r a m e _   =   f a l s e ; 
 
                 }   e l s e   { 
 
                         r e t u r n   f a l s e ;     / /   �m 2}ȕ�p�S����b
 
                 } 
 
         } 
 
         r e t u r n   s e n d V i d e o F r a m e ( s a m p l e ) ; 
 
 } 
 
 ` ` ` 
 
 
 
 # # #   1 0 . 6   K��0�`�Y� ̓? 
 
 
 
 ` ` ` c p p 
 
 c l a s s   H e a r t b e a t M a n a g e r   { 
 
         i n t   i n t e r v a l M s _   =   5 0 0 0 ;       / /   5 �~�cB_��wO�zZ? 
 
         i n t   t i m e o u t M s _   =   1 5 0 0 0 ;       / /   1 5 �~�c�h]��]2|�tnTi
 
 } ; 
 
 ` ` ` 
 
 
 
 # # #   1 0 . 7   �qx�m�^m��? 
 
 
 
 ` ` ` c p p 
 
 s t r u c t   R t m p S t a t s   { 
 
         i n t 6 4 _ t   v i d e o F r a m e s S e n t   =   0 ; 
 
         i n t 6 4 _ t   a u d i o F r a m e s S e n t   =   0 ; 
 
         i n t 6 4 _ t   b y t e s S e n t   =   0 ; 
 
         i n t 6 4 _ t   v i d e o Q u e u e D e l a y M s   =   0 ; 
 
         i n t   g o p D r o p p e d   =   0 ; 
 
         d o u b l e   f p s   =   0 . 0 ; 
 
 } ; 
 
 
 
 p u b l i s h e r . o n S t a t s ( [ ] ( c o n s t   R t m p S t a t s &   s t a t s )   { 
 
         p r i n t f ( " F r a m e s :   % l l d ,   D e l a y :   % l l d   m s ,   G O P   d r o p p e d :   % d \ n " ,   
 
                       s t a t s . v i d e o F r a m e s S e n t ,   s t a t s . v i d e o Q u e u e D e l a y M s ,   s t a t s . g o p D r o p p e d ) ; 
 
 } ) ; 
 
 ` ` ` 
 
 
 
 # # #   1 0 . 8   Ó�0T~�m�^�v�t? 
 
 
 
 ` ` ` c p p 
 
 L O G _ I N F O ( " C o n n e c t i n g   t o   { } : { } " ,   h o s t ,   p o r t ) ; 
 
 L O G _ E R R O R ( " C o n n e c t i o n   f a i l e d :   { } " ,   e r r o r ) ; 
 
 L O G _ W A R N ( " Q u e u e   d e l a y   h i g h :   { } m s " ,   d e l a y ) ; 
 
 L O G _ D E B U G ( " S e n t   v i d e o   f r a m e :   s i z e = { } ,   t s = { } " ,   s i z e ,   t s ) ; 
 
 ` ` ` 
 
 
 
 - - - 
 
 
 
 # #   ��d��|
 
 
 
 ȓ�g�YF�\Q)�&h!|R T M P ��&1fyS D K (�? 0 �mE�srG��Q�[�hHj0}
 
 
 
 1 .   R T M P W��_��:   ��!�X��Nh u n k R��Uao��yO�y��⫈h�? 
 
 2 .   F L V Op}O��:   Yt�Uv�T a g ���Ovq�hcaa g 
 
 3 .   H . 2 6 4 +hr:   N A L U ��<NP S / P P S ��NV C C ͓Nq!}
 
 4 .   A A C +hr:   A D T S ��Nu d i o S p e c i f i c C o n f i g 
 
 5 .   A M F 0 +hr:   ��HrA]�~�7p��xO��t3 -|R�@i�[
 
 6 .    b�||m�r�}:   ȕ�pj�o�oO ��wO(|`m���`�? 
 
 7 .   ÓX�h���Q�`�Y?   P T S / D T S ���OvqYt�Uv�5pR_�}
 
 8 .   �5�� yO�n:   |m3l=v�5�� uO� }O�yÓ5�6^R�? 
 
 9 .   s�}\^��C�W:   G O P �m 2�b��xO�Y��zOr�? 
 
 1 0 .   ��k��o�R�`:   ���V�f��� ��䈬 �OxVig�p�t#�? 
 
 
 
 # # #   ͓?z>~UtzOcP
 
 
 
 -   R T M P cm�de�m
Y0w˓5�/p�K[h u n k 9p�pG^�o3l�w�o�]de
 
 -   H . 2 6 4 "��V C C ͓Nq!}�I[A C X�ZZ�jA D T S �o? 
 
 -   ÓX�h��EQ�|0 �[� �o6[}C o m p o s i t i o n T i m e   =   P T S   -   D T S 
 
 -   G O P ���qaq�m 2�b���W�Sz�^SFw
 
 -   ���V�f��� ��WWxVig�p)OO��]GnT�3 �jƕD�z
 
 
 
 # # #   Z��^;u�[?���
 
 
 
 1 .   * * 9p�pԏ`i�\	v* * :   5p-W�SgZ*a,U.�zO�`YtE��`�Y3 ao9p�pG^
 
 2 .   * * ��FXQ�t�Q-}* * :   �t�Q�fÕ�q�W�oC�v��NO P ���W�[
 
 3 .   * * �[�P6r4Z-[/v* * :   �Y!�Z b�|�[�P6r��\Ys�t�OxVig? 
 
 4 .   * * W��_�ᵓA%Mw* * :   ����[R T M P S �� N. 2 6 5 �~? 
 
 
 
 /u~\\nigk$U9p~\�f(��R�Yȓ��[W�DiXQ/u���Ycm�rA~O��0	^����T M P ��� ȓ$�}Op�UV~�msD K *��q�Y�;j�Wcm�r�V�[���k�m�n�0�? 
 
 