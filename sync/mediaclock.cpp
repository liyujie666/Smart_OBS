#include "mediaclock.h"
#include <QDebug>
MediaClock::MediaClock() {}


void MediaClock::setMasterClockType(MasterClockType type) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (masterType_ == type) return;  // 相同类型无需切换

    // 记录旧基准，切换新基准
    MasterClockType oldType = masterType_;
    masterType_ = type;

    int64_t nowTime = GlobalClock::getInstance().getCurrentUs();
    switch (type) {
    case MasterClockType::Audio:
        if (lastAPts_ != 0) {
            startTime_ = nowTime - lastAPts_;
        }
        break;
    case MasterClockType::Video:
        if (lastVPts_ != 0) {
            startTime_ = nowTime - lastVPts_;
        }
        break;
    case MasterClockType::External:
        if (lastExtPts_ != 0) {
            startTime_ = nowTime - lastExtPts_;
        }
        break;
    }

    totalPauseUs_ = 0;
    isPaused_ = false;
    qDebug() << "[AVSyncClock] switch master to" << (int)type << ", startTime_ reset to" << startTime_;
}
// int64_t MediaClock::setMediaAPts(int64_t aPts)
// {
//     std::lock_guard<std::mutex> lock(mtx_);
//     int64_t nowTime = GlobalClock::getInstance().getCurrentUs();

//     // 音频首帧初始化（仅在音频基准时生效）
//     if (isFirstAFrame_) {
//         isFirstAFrame_ = false;
//         if (masterType_ == MasterClockType::Audio) {
//             startTime_ = nowTime - (aPts / speed_);  // 锚定时钟起点
//             totalPauseUs_ = 0;
//             isPaused_ = false;
//             qDebug() << "[AVSyncClock] first audio frame (audio master), startTime_ set to" << startTime_;
//         }
//         lastAPts_ = aPts;
//         return 0;
//     }

//     lastAPts_ = aPts;
//     if (masterType_ != MasterClockType::Audio) return 0;


//     // 计算偏差、重锚定
//     int64_t offsetTime = (nowTime - totalPauseUs_) - startTime_;
//     int64_t diffUs = aPts - offsetTime * speed_;

//     const int64_t MAX_SLEEP_US = 50000;
//     const int64_t REANCHOR_THRESHOLD_US = 200000 * speed_;

//     if (diffUs > REANCHOR_THRESHOLD_US || diffUs < -REANCHOR_THRESHOLD_US) {
//         startTime_ = nowTime - (aPts / speed_);
//         totalPauseUs_ = 0;
//         diffUs = 0;
//         qDebug() << "[AVSyncClock] audio large drift, re-anchoring to pts=" << aPts;
//     } else if (diffUs > MAX_SLEEP_US) {
//         diffUs = MAX_SLEEP_US;
//     }

//     return diffUs;
// }

int64_t MediaClock::setMediaAPts(int64_t aPts)
{
    std::lock_guard<std::mutex> lock(mtx_);
    int64_t nowTime = GlobalClock::getInstance().getCurrentUs();

    if (isFirstAFrame_) {
        isFirstAFrame_ = false;
        if (masterType_ == MasterClockType::Audio) {
            startTime_ = nowTime - aPts;
            totalPauseUs_ = 0;
            isPaused_ = false;
            qDebug() << "[AVSyncClock] first audio frame (audio master), startTime_ set to" << startTime_;
        }
        lastAPts_ = aPts;
        return 0;
    }

    lastAPts_ = aPts;
    // qDebug()  << "audio frame pts:" << lastAPts_;
    if (masterType_ != MasterClockType::Audio) return 0;

    int64_t offsetTime = (nowTime - totalPauseUs_) - startTime_;
    int64_t diffUs = aPts - offsetTime;

    const int64_t MAX_SLEEP_US = 50000;
    const int64_t REANCHOR_THRESHOLD_US = 200000;

    if (abs(diffUs) > REANCHOR_THRESHOLD_US) {
        startTime_ = nowTime - aPts;
        totalPauseUs_ = 0;
        diffUs = 0;
        qDebug() << "[AVSyncClock] audio large drift, re-anchoring to pts=" << aPts
                 << "diff=" << diffUs;
    } else if (diffUs > MAX_SLEEP_US) {
        diffUs = MAX_SLEEP_US;
    }

    return diffUs;
}


int64_t MediaClock::setMediaVPts(int64_t vPts) {
    std::lock_guard<std::mutex> lock(mtx_);
    int64_t nowTime = GlobalClock::getInstance().getCurrentUs();

    if (isFirstVFrame_) {
        isFirstVFrame_ = false;
        if (masterType_ == MasterClockType::Video) {
            startTime_ = nowTime - vPts;  // 锚定时钟起点
            totalPauseUs_ = 0;
            isPaused_ = false;
            qDebug() << "[AVSyncClock] first video frame (video master), startTime_ set to" << startTime_;
        }
        lastVPts_ = vPts;
        return 0;
    }

    lastVPts_ = vPts;
    if (masterType_ != MasterClockType::Video) return 0;


    int64_t offsetTime = (nowTime - totalPauseUs_) - startTime_;
    int64_t diffUs = vPts - offsetTime;

    const int64_t MAX_SLEEP_US = 50000;
    const int64_t REANCHOR_THRESHOLD_US = 200000;

    if (diffUs > REANCHOR_THRESHOLD_US || diffUs < -REANCHOR_THRESHOLD_US) {
        startTime_ = nowTime - vPts;
        totalPauseUs_ = 0;
        diffUs = 0;
        qDebug() << "[AVSyncClock] video large drift, re-anchoring to pts=" << vPts;
    } else if (diffUs > MAX_SLEEP_US) {
        diffUs = MAX_SLEEP_US;
    }

    return diffUs;
}


void MediaClock::setExternalPts(int64_t extPts) {
    std::lock_guard<std::mutex> lock(mtx_);
    lastExtPts_ = extPts;  // 更新外部PTS

    if (masterType_ == MasterClockType::External) {
        int64_t nowTime = GlobalClock::getInstance().getCurrentUs();
        startTime_ = nowTime - extPts;  // 直接锚定到外部PTS
        totalPauseUs_ = 0;
        isPaused_ = false;
        qDebug() << "[AVSyncClock] external clock updated, startTime_ set to" << startTime_;
    }
}

void MediaClock::setSpeed(double speed)
{
    if (qFuzzyCompare(speed_, speed)) return;  // 速度未变则返回

    speed_ = speed;
    int64_t now = GlobalClock::getInstance().getCurrentUs();
    int64_t currentPts = getMaterClockPts();

    if (currentPts != 0) {
        int64_t adjustedNow = now - totalPauseUs_;  // 扣除暂停时间
        startTime_ = adjustedNow - currentPts;
    }
}
int64_t MediaClock::getMaterClockPts() {
    std::lock_guard<std::mutex> lock(mtx_);
    // 根据当前基准返回对应PTS
    switch (masterType_) {
    case MasterClockType::Audio:    return lastAPts_;
    case MasterClockType::Video:    return lastVPts_;
    case MasterClockType::External: return lastExtPts_;
    default: return lastAPts_;
    }
}

void MediaClock::pause()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if(!isPaused_)
    {
        isPaused_ = true;
        pauseStartUs_ = GlobalClock::getInstance().getCurrentUs();
        qDebug() << "[MediaClock] paused " << "pauseStartUs_ : " << pauseStartUs_;
    }
}

void MediaClock::resume()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if(isPaused_)
    {
        isPaused_ = false;
        totalPauseUs_ += GlobalClock::getInstance().getCurrentUs() - pauseStartUs_;
        qDebug() << "[MediaClock] paused " << "totalPauseUs_ : " << totalPauseUs_;
    }

}

void MediaClock::setVideoStarted(bool started) {
    std::lock_guard<std::mutex> lock(mtx_);
    isVideoStarted_ = started;
    if (started) cv_.notify_all();
}

bool MediaClock::isVideoStarted() {
    std::lock_guard<std::mutex> lock(mtx_);
    return isVideoStarted_;
}

bool MediaClock::waitForVideoStart(int64_t timeoutUs) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, std::chrono::microseconds(timeoutUs),
                        [this]() { return isVideoStarted_; });
}

void MediaClock::resetMedia() {
    std::lock_guard<std::mutex> lock(mtx_);
    masterType_ = MasterClockType::Audio;  // 重置为默认音频基准
    // 重置所有基准的PTS和首帧标记
    lastAPts_ = 0;  isFirstAFrame_ = true;
    lastVPts_ = 0;  isFirstVFrame_ = true;
    lastExtPts_ = 0;
    // 重置公共时钟变量
    startTime_ = 0;
    totalPauseUs_ = 0;
    isPaused_ = false;
    pauseStartUs_ = 0;
    // 视频同步相关重置
    isVideoStarted_ = false;
    isVideoFirstFrameReady_ = false;
    qDebug() << "[AVSyncClock] reset media";
}


void MediaClock::seekTo(int64_t ptsUs) {
    std::lock_guard<std::mutex> lock(mtx_);
    int64_t now = GlobalClock::getInstance().getCurrentUs();
    startTime_ = now - ptsUs;  // 锚定到目标PTS
    totalPauseUs_ = 0;
    isPaused_ = false;
    // 重置所有基准的PTS（统一为seek目标值）
    lastAPts_ = ptsUs;
    lastVPts_ = ptsUs;
    lastExtPts_ = ptsUs;
    // 重置首帧标记（seek后视为新开始）
    isFirstAFrame_ = false;
    isFirstVFrame_ = false;
    qDebug() << "[AVSyncClock] seekTo:" << ptsUs << "startTime_=" << startTime_;
}
void MediaClock::setVideoFirstFrameReady() {
    std::lock_guard<std::mutex> lock(mtx_);
    isVideoFirstFrameReady_ = true;
    videoFirstFrameCond_.notify_all(); // 通知等待的音频线程
    qDebug() << "[MediaClock] video first frame is ready";
}

bool MediaClock::waitForVideoFirstFrame(int64_t timeoutUs) {
    std::unique_lock<std::mutex> lock(mtx_);
    return videoFirstFrameCond_.wait_for(lock,
                                         std::chrono::microseconds(timeoutUs),
                                         [this]() { return isVideoFirstFrameReady_; });
}

void MediaClock::resetVideoFirstFrame() {
    std::lock_guard<std::mutex> lock(mtx_);
    isVideoFirstFrameReady_ = false;
}
