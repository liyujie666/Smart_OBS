## 4. AAC音频编码格式

### 4.1 AAC基础

**AAC (Advanced Audio Coding)** 是MPEG-4标准的音频编码格式，相比MP3在同码率下音质更好。

#### AAC Profile

| Profile | 复杂度 | 应用场景 |
|---------|--------|----------|
| AAC-LC  | Low    | 最常用，直播推流标配 |
| AAC-Main | High  | 高音质，编码慢 |
| AAC-HE  | Medium | 低码率(32-64 kbps) |
| AAC-HEv2 | Medium | 超低码率(16-32 kbps) |

**直播推流推荐**: AAC-LC, 44.1kHz, Stereo, 128kbps

---

### 4.2 ADTS vs Raw AAC

#### ADTS (Audio Data Transport Stream)

带同步头的AAC格式，每个帧独立解码:

```
[ADTS Header 7B][AAC Frame][ADTS Header 7B][AAC Frame] ...
```

**ADTS Header结构** (7字节，无CRC):

```
Byte 0        Byte 1        Byte 2        Byte 3        Byte 4        Byte 5        Byte 6
11111111      11110BCC      DDDDEEEE      FGGGGHHH      HHHHHHHH      HHHHHIII      IIIIIIII
[syncword前8位] [后4位+其他] [采样率+声道]   [帧长度高位]   [帧长度中位]   [帧长度低位]   [buffer_fullness]
```

**详细位分布**:

```
 Bit位:  0  1  2  3  4  5  6  7 | 8  9 10 11 12 13 14 15 |16 17 18 19 20 21 22 23 |...
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
字段:  |     syncword (12位)     |ID|layer|pa|profile|freq idx |pb|  channel  |...
       +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
       |   固定 0xFFF (同步字)    |1 | 00  |1 | 01~02 | 0~15   |0 |  1~7      |
```

**字段详解**:

| 字段 | 起始位 | 位数 | 说明 | 示例 |
|------|--------|------|------|------|
| syncword | 0 | 12 | 固定 `0xFFF`，用于帧同步 | `1111 1111 1111` |
| ID | 12 | 1 | 0=MPEG-4, 1=MPEG-2 | `0` |
| layer | 13 | 2 | 固定 `00` | `00` |
| protection_absent | 15 | 1 | 1=无CRC(7字节), 0=有CRC(9字节) | `1` |
| profile | 16 | 2 | `audioObjectType - 1`<br>0=Main, 1=LC, 2=SSR | `1` (AAC-LC) |
| sampling_frequency_index | 18 | 4 | 采样率索引(见下表) | `4` (44.1kHz) |
| private_bit | 22 | 1 | 通常为0 | `0` |
| channel_configuration | 23 | 3 | 1=Mono, 2=Stereo | `2` |
| originality | 26 | 1 | 通常为0 | `0` |
| home | 27 | 1 | 通常为0 | `0` |
| copyrighted | 28 | 1 | 通常为0 | `0` |
| copyright_start | 29 | 1 | 通常为0 | `0` |
| frame_length | 30 | 13 | **包含ADTS头**的总帧长 | 如 `1024` |
| buffer_fullness | 43 | 11 | 0x7FF=VBR | `0x7FF` |
| num_raw_data_blocks | 54 | 2 | 通常为0 | `0` |

**实际例子** (AAC-LC, 44.1kHz, Stereo, 帧长=1024):

```
字节:    FF  F1  50  80  04  00  1F  FC
二进制:  11111111 11110001 01010000 10000000 00000100 00000000 00011111 11111100
         |syncword|I|ly|pa|prof|freq|p|chan|or|h|c|cs|  frame_length   |buffer_full|blks|
         |  0xFFF |0|00|1 |01  |0100|0|010 |0|0|0|0 |    0x0404=1028  |   0x7FF   | 0  |
```

解读:
- syncword = `0xFFF` ✓
- ID = `0` (MPEG-4 AAC)
- protection_absent = `1` (无CRC，7字节头)
- profile = `1` (AAC-LC，因为 audioObjectType=2, 所以 2-1=1)
- sampling_frequency_index = `4` (44100 Hz)
- channel_configuration = `2` (Stereo)
- frame_length = `1028` 字节 (包含7字节ADTS头 + 1021字节AAC数据)

#### Raw AAC

不含同步头，只有纯AAC Access Unit数据。

**RTMP要求Raw AAC**，需要去掉ADTS头:

```cpp
// 去除ADTS头
const uint8_t* adtsFrame = ...;
int adtsHeaderSize = ((adtsFrame[1] & 0x01) == 0) ? 9 : 7;  // 有无CRC
const uint8_t* rawAAC = adtsFrame + adtsHeaderSize;

// 从ADTS头解析帧总长度
size_t frameLength = ((adtsFrame[3] & 0x03) << 11) |
                     (adtsFrame[4] << 3) |
                     ((adtsFrame[5] & 0xE0) >> 5);
size_t rawSize = frameLength - adtsHeaderSize;
```

---

### 4.3 AudioSpecificConfig (ASC) 详解

AAC Sequence Header中包含的 **AudioSpecificConfig**，通常2字节，描述音频参数。

**ASC位布局**:

```
 Bit 15..11   Bit 10..7              Bit 6..3          Bit 2..0
+------------+----------------------+-----------------+---------+
| audioObjTy |  samplingFreqIndex   | channelConfig   | flags   |
|  (5 bits)  |      (4 bits)        |   (4 bits)      | (3 bits)|
+------------+----------------------+-----------------+---------+
```

**字段说明**:

| 字段 | 位数 | 常用值 |
|------|------|--------|
| audioObjectType | 5 | 2=AAC-LC, 5=AAC-HE |
| samplingFrequencyIndex | 4 | 见下表 |
| channelConfiguration | 4 | 1=Mono, 2=Stereo, 6=5.1 |
| frameLengthFlag | 1 | 0=1024 samples |
| dependsOnCoreCoder | 1 | 通常为0 |
| extensionFlag | 1 | 通常为0 |

**采样率索引表**:

| Index | 采样率 |
|-------|--------|
| 0x3 | 48000 Hz |
| 0x4 | 44100 Hz |
| 0x5 | 32000 Hz |
| 0x6 | 24000 Hz |
| 0x8 | 16000 Hz |
| 0xB | 8000 Hz |

**计算示例 (AAC-LC, 44.1kHz, Stereo)**:

```
audioObjectType      = 2  (AAC-LC)
samplingFrequencyIndex = 4  (44100 Hz)
channelConfiguration = 2  (Stereo)

Byte 1: [audioObjTy(5)] [samplingFreqIdx高3位(3)]
      = [00010] [010] = 0001 0010 = 0x12

Byte 2: [samplingFreqIdx低1位(1)] [channelConfig(4)] [flags(3)]
      = [0] [0010] [000] = 0001 0000 = 0x10

ASC = [0x12, 0x10]
```

**代码实现**:

```cpp
// 构造 AudioSpecificConfig
std::vector<uint8_t> buildASC(int sampleRate, int channels) {
    // audioObjectType = 2 (AAC-LC)
    int aot = 2;

    // 采样率索引
    static const int freqTable[] = {
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000
    };
    int freqIdx = 4;  // 默认44100
    for (int i = 0; i < 12; i++) {
        if (freqTable[i] == sampleRate) { freqIdx = i; break; }
    }

    uint8_t byte1 = (aot << 3) | (freqIdx >> 1);
    uint8_t byte2 = ((freqIdx & 0x1) << 7) | (channels << 3);
    return {byte1, byte2};
}

// 解析 AudioSpecificConfig
void parseASC(const uint8_t* asc) {
    int aot      = (asc[0] >> 3) & 0x1F;
    int freqIdx  = ((asc[0] & 0x07) << 1) | ((asc[1] >> 7) & 0x01);
    int channels = (asc[1] >> 3) & 0x0F;
    printf("Profile: %d, FreqIdx: %d, Channels: %d\n", aot, freqIdx, channels);
}
```

---

### 4.4 FLV中的音频Tag格式

RTMP音频消息的payload格式:

```
+--------+--------+-----...-----+
| Byte 0 | Byte 1 |    Data     |
+--------+--------+-----...-----+
```

**Byte 0: SoundFormat + SoundRate + SoundSize + SoundType**

```
 Bit 7..4    Bit 3..2   Bit 1   Bit 0
+-----------+----------+-------+-------+
| SoundFmt  | SoundRate| SndSz | SndTy |
| (4 bits)  | (2 bits) |(1 bit)|(1 bit)|
+-----------+----------+-------+-------+
```

| 字段 | 值 | 说明 |
|------|----|------|
| SoundFormat | 10 | AAC |
| SoundRate | 3 | 44kHz (AAC时固定填3) |
| SoundSize | 1 | 16-bit (AAC时固定填1) |
| SoundType | 1 | Stereo (AAC时固定填1) |

AAC固定值: `(10<<4) | (3<<2) | (1<<1) | 1 = 0xAF`

**Byte 1: AACPacketType**

| 值 | 类型 |
|----|------|
| 0 | AAC Sequence Header (AudioSpecificConfig) |
| 1 | AAC Raw (音频帧数据) |
