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
