
### 9.3 分层队列架构

你的SDK使用分离的音视频队列(`bounded_queue.h`):

```cpp
class DualQueue {
    BoundedQueue videoQueue_;
    BoundedQueue audioQueue_;
    PublisherConfig config_;

public:
    bool pushVideo(const MediaSample& sample) {
        int64_t delayMs = videoQueue_.getMediaDelayUs() / 1000;
        if (delayMs > config_.maxQueueDelayMs) {
            if (dropOldestGop()) {
                stats_.gopDropped++;
                requestKeyframe();
            }
        }
        return videoQueue_.push(sample);
    }

    bool pushAudio(const MediaSample& sample) {
        int64_t delayMs = audioQueue_.getMediaDelayUs() / 1000;
        if (delayMs > config_.maxAudioQueueMs) {
            audioQueue_.popFront();
        }
        return audioQueue_.push(sample);
    }
};
```

**设计意图**:
- 音频和视频分开管理，互不干扰
- 视频可以丢整个GOP，音频丢单帧
- 避免视频积压影响音频实时性

### 9.4 GOP感知丢帧策略

#### 为什么要按GOP丢帧

直接丢单帧会导致解码器参考帧缺失，产生花屏。正确做法是丢弃完整的GOP：

```
丢单帧 (错误):
IDR P P [丢P] P P IDR ...
              ↑ 后续P帧参考了被丢的帧 → 花屏

丢整个GOP (正确):
[IDR P P P P] IDR P P P ...
 ↑ 整个GOP丢弃  ↑ 从新IDR开始，解码正常
```

#### GOP边界检测实现

```cpp
struct GOP {
    std::vector<MediaSample> frames;
    int64_t startDts = 0;
    int64_t endDts = 0;
    bool complete = false;
};

class GopQueue {
    std::deque<GOP> gops_;
    GOP currentGop_;

public:
    void push(const MediaSample& sample) {
        if (sample.type == SampleType::VideoKey) {
            if (!currentGop_.frames.empty()) {
                currentGop_.complete = true;
                currentGop_.endDts = currentGop_.frames.back().dtsUs;
                gops_.push_back(std::move(currentGop_));
            }
            currentGop_ = GOP();
            currentGop_.startDts = sample.dtsUs;
        }
        currentGop_.frames.push_back(sample);
    }

    bool dropOldestGop() {
        if (gops_.empty()) return false;
        GOP& oldest = gops_.front();
        LOG_WARN("Dropping GOP: {} frames, {}ms",
                 oldest.frames.size(),
                 (oldest.endDts - oldest.startDts) / 1000);
        gops_.pop_front();
        return true;
    }
};
```

#### 三档阈值策略

对应你的SDK配置(`PublisherConfig`):

```
maxQueueDelayMs    = 800ms   → 中度积压，降码率
dropGopThresholdMs = 1200ms  → 严重积压，丢GOP
```

```cpp
void handleBackpressure() {
    int64_t delayMs = gopQueue_.getTotalDelayMs();

    if (delayMs > config_.dropGopThresholdMs) {
        if (gopQueue_.dropOldestGop()) {
            stats_.gopDropped++;
            requestKeyframe();
        }
    } else if (delayMs > config_.maxQueueDelayMs) {
        int targetBps = currentBps_ * 0.8;
        if (bitrateCallback_) bitrateCallback_(targetBps);
    } else if (delayMs < config_.maxQueueDelayMs / 2) {
        int targetBps = std::min((int)(currentBps_ * 1.1), maxBps_);
        if (bitrateCallback_) bitrateCallback_(targetBps);
    }
}
```

### 9.5 关键帧请求机制

你的SDK通过回调通知编码器产生关键帧:

```cpp
publisher.onRequestKeyframe([&encoder]() {
    encoder.forceKeyframe();
});

// C接口
rtmp_on_request_keyframe(p, [](void* user) {
    static_cast<Encoder*>(user)->forceKeyframe();
}, &encoder);
```

#### 请求频率限制

```cpp
class KeyframeRequestLimiter {
    std::chrono::steady_clock::time_point lastRequestTime_;
    int minIntervalMs_ = 2000;

public:
    bool tryRequest() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastRequestTime_).count();
        if (elapsed < minIntervalMs_) return false;
        lastRequestTime_ = now;
        return true;
    }
};
```
