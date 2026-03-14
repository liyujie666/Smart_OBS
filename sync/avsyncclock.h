// #ifndef AV_SYNC_CLOCK_H
// #define AV_SYNC_CLOCK_H

// #include <cstdint>
// #include <chrono>
// #include <mutex>
// // 同步模式：音频主导/视频主导/外部时钟
// enum class ClkMode { None, AMaster, VMaster, External };

// class AVSyncClock {
// public:
//     AVSyncClock() = default;
//     ~AVSyncClock() = default;


//     void initAudio(int sampleRate, int nbSamples);

//     void initVideo(int fps);

//     void start(ClkMode mode);

//     int64_t getAPts();

//     int64_t getVPts();

//     bool isValid() const { return mode_ != ClkMode::None; }

//     bool isInitialized() const { return aInitialized; }

// private:
//     // 获取单调时钟（微秒，永不回溯）
//     int64_t nowUs() const {
//         using namespace std::chrono;
//         return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
//     }

// private:
//     ClkMode mode_ = ClkMode::None;  // 同步模式
//     int64_t startUs_ = 0;           // 启动时的单调时间（微秒）

//     // 音频参数
//     int64_t aPts_ = 0;              // 音频当前PTS
//     double aDur_ = 0;               // 每帧音频持续时间（微秒）
//     int64_t lastAPts_ = 0;          // 上一次音频PTS（防重复）

//     // 视频参数
//     int64_t vPts_ = 0;              // 视频当前PTS
//     double vDur_ = 0;               // 每帧视频持续时间（微秒）
//     int64_t lastVPts_ = 0;          // 上一次视频PTS（防重复）

//     int64_t lastAFrameTimeUs_ = 0;
//     int64_t lastVFrameTimeUs_ = 0;

//     bool aInitialized = false;
//     int64_t vFrameCount_ = 0;
//     int64_t aFrameCount_ = 0;

//     std::mutex vpts_mutex_;

// };


// #endif // AV_SYNC_CLOCK_H

#ifndef AV_SYNC_CLOCK_H
#define AV_SYNC_CLOCK_H
#include "globalclock.h"
#include <cstdint>
#include <chrono>
#include <mutex>
#include <cmath>
#include <QDebug>
#include <deque>

enum class ClkMode {
    None,       // 未初始化
    Relative,   // 相对时间模式（从0开始递增）
    Realtime   // 实时时间模式（基于系统时钟）
};

class AVSyncClock {
public:
    AVSyncClock() = default;
    ~AVSyncClock() = default;

    void initAudio(int sampleRate, int nbSamples);
    void initVideo(int fps);
    void start(ClkMode mode);
    void reset();
    void pause();
    void resume();

    int64_t getAPts();
    int64_t getVPts();
    void calibrateAudio(int64_t externalTsUs);
    void calibrateVideo(int64_t externalTsUs);

    int64_t getStartUs() const { return startUs_; }
    void setMaxAllowedDiff(int64_t maxDiffUs) { maxAllowedDiffUs_ = maxDiffUs; }
    void setAPts(int64_t pts);
    ClkMode getMode() const { return mode_; }
    int64_t getLastAPts() const { return aLastPts_; }
    bool isValid() const { return mode_ != ClkMode::None; }
    int64_t getVideoFrameDurUs() {
        return static_cast<int64_t>(round(vFrameDurUs_));
    }

    // 修正视频帧计数（解决PTS回退时的累积问题）
    void setVideoFrameCount(int64_t count) {
        std::lock_guard<std::mutex> lock(mtx_);
        vFrameCount_ = count;
        vPts_ = vFrameCount_ * static_cast<int64_t>(round(vFrameDurUs_));
    }



private:
    ClkMode mode_ = ClkMode::None;       // 同步模式
    int64_t startUs_ = 0;                // 启动时的系统时间（微秒）
    int64_t maxAllowedDiffUs_ = 50000;   // 最大允许偏差（默认50ms）
    std::atomic<bool> isPaused_;
    int64_t pauseStartUs_ = 0;
    int64_t pauseDuration_ = 0;
    // 音频参数
    int64_t aPts_ = 0;                   // 音频当前PTS（微秒）
    double aFrameDurUs_ = 0;             // 每帧音频持续时间（微秒）
    int64_t aLastExternalTs_ = 0;        // 上一次音频外部校准时间戳
    bool aFirstExternal_ = true;
    int64_t aLastPts_ = 0;
    int64_t aFrameCount_ = 0;
    int64_t lastCalibrateUs_ = 0;
    double aCompensatePerFrameUs_ = 0; // 每帧需额外补偿的微秒数
    int64_t aTotalCompensateUs_ = 0;   // 剩余需补偿的总微秒数
    const int64_t audioOffset = 500000;


    // 视频参数
    int64_t vPts_ = 0;                   // 视频当前PTS（微秒）
    double vFrameDurUs_ = 0;             // 每帧视频持续时间（微秒）
    int64_t vLastExternalTs_ = 0;        // 上一次视频外部校准时间戳
    bool vFirstExternal_ = true;
    int64_t vFrameCount_ = 0;
    bool isFirstAudioFrame_ = true;
    std::mutex mtx_;                     // 线程安全锁

    // 计算实时模式下的基准时间（微秒）
    int64_t getRealTimeBase() const {
        int64_t currentUs = GlobalClock::getInstance().getCurrentUs();
        int64_t elapsedUs = currentUs - startUs_ - pauseDuration_;
        return std::max<int64_t>(0,elapsedUs);
    }
};

#endif // AV_SYNC_CLOCK_H



