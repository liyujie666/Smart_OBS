#ifndef MEDIACLOCK_H
#define MEDIACLOCK_H
#include "globalclock.h"
#include <chrono>
#include <mutex>
enum class MasterClockType {
    Audio,   // 以音频PTS为主时钟（默认）
    Video,   // 以视频PTS为主时钟（备用）
    External // 外部时钟（如系统时钟，暂不考虑）
};
class MediaClock
{
public:
    MediaClock();

    void setMasterClockType(MasterClockType type);
    int64_t setMediaAPts(int64_t aPts);
    int64_t setMediaVPts(int64_t vPts);
    void setExternalPts(int64_t extPts);
    void setSpeed(double speed);
    int64_t getMaterClockPts();
    void pause();
    void resume();
    void setVideoStarted(bool started);
    bool isVideoStarted();
    bool waitForVideoStart(int64_t timeoutUs = 5000000);
    void resetMedia();
    void seekTo(int64_t ptsUs);
    void setVideoFirstFrameReady();
    bool waitForVideoFirstFrame(int64_t timeoutUs);
    void resetVideoFirstFrame();

private:
    MasterClockType masterType_ = MasterClockType::Audio;

    // 音频
    int64_t lastAPts_ = 0;
    bool isFirstAFrame_ = true;

    // 视频
    int64_t lastVPts_ = 0;
    bool isFirstVFrame_ = true;

    // 外部时钟
    int64_t lastExtPts_ = 0;

    // 公共
    int64_t startTime_ = 0;        // 时钟起始时间
    int64_t totalPauseUs_ = 0;     // 累计暂停时间
    bool isPaused_ = false;        // 暂停状态
    int64_t pauseStartUs_ = 0;     // 暂停开始时间
    // 视频同步
    bool isVideoStarted_ = false;
    bool isVideoFirstFrameReady_ = false;
    std::condition_variable cv_;
    std::condition_variable videoFirstFrameCond_;
    std::mutex mtx_;

    double speed_ = 1.0;
};

#endif // MEDIACLOCK_H
