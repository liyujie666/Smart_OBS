#include "avsyncclock.h"
#include <QDebug>

void AVSyncClock::initAudio(int sampleRate, int nbSamples) {
    std::lock_guard<std::mutex> lock(mtx_);
    aFrameDurUs_ = (1000000.0 * nbSamples) / sampleRate;
    qDebug() << "音频帧时长初始化：" << aFrameDurUs_ << "微秒（"
             << sampleRate << "Hz，" << nbSamples << "样本）";
}

void AVSyncClock::initVideo(int fps) {
    std::lock_guard<std::mutex> lock(mtx_);
    vFrameDurUs_ = 1000000.0 / fps;
    if (fps > 50) { // 对高帧率视频使用更小的阈值
        maxAllowedDiffUs_ = 20000; // 20ms（约1.2帧@60fps）
    }
    qDebug() << "视频帧时长初始化：" << vFrameDurUs_ << "微秒（" << fps << "FPS）";
}

// 启动时钟
void AVSyncClock::start(ClkMode mode) {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = mode;
    startUs_ = GlobalClock::getInstance().getCurrentUs();
    pauseDuration_ = 0;
    aPts_ = 0;
    vPts_ = static_cast<int64_t>(round(vFrameDurUs_));;
    aFrameCount_ = 0;
    vFrameCount_ = 0;
    aLastExternalTs_ = 0;
    aLastPts_ = 0;
    vLastExternalTs_ = 0;
    aCompensatePerFrameUs_ = 0;
    aTotalCompensateUs_ = 0;
    isPaused_.store(false);
    qDebug() << "同步时钟启动：模式=" << static_cast<int>(mode)
             << "，启动时间=" << startUs_;
}

// 重置时钟
// void AVSyncClock::reset() {
//     std::lock_guard<std::mutex> lock(mtx_);
//     startUs_ = GlobalClock::getInstance().getCurrentUs();
//     aPts_ = 0;
//     vPts_ = 0;
//     mode_ = ClkMode::None;
//     aCompensatePerFrameUs_ = 0;
//     aTotalCompensateUs_ = 0;
//     qDebug() << "同步时钟重置：新启动时间=" << startUs_;
// }


void AVSyncClock::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    mode_ = ClkMode::None;       // 重置模式
    startUs_ = 0;                // 重置启动时间
    maxAllowedDiffUs_ = 50000;   // 重置最大偏差
    pauseDuration_ = 0;   // 重置累计暂停时间
    isPaused_.store(false);

    // 重置音频时间戳
    aPts_ = 0;
    aFrameDurUs_ = 0;
    aLastExternalTs_ = 0;
    aFirstExternal_ = true;
    aLastPts_ = 0;
    aFrameCount_ = 0;
    aCompensatePerFrameUs_ = 0;
    aTotalCompensateUs_ = 0;

    // 重置视频时间戳
    vPts_ = 0;
    vFrameDurUs_ = 0;
    vLastExternalTs_ = 0;
    vFirstExternal_ = true;
    vFrameCount_ = 0;
    isFirstAudioFrame_ = true;
}

void AVSyncClock::pause()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if(isPaused_.load() || !isValid()) return;

    pauseStartUs_ = GlobalClock::getInstance().getCurrentUs();
    isPaused_.store(true);
}

void AVSyncClock::resume()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (!isPaused_.load() || !isValid()) return;

    int64_t resumetUs = GlobalClock::getInstance().getCurrentUs();
    int64_t elapsedUs = resumetUs - pauseStartUs_;
    pauseDuration_ += elapsedUs;
    qDebug() << "暂停时长（累计）：" << pauseDuration_;
    isPaused_.store(false);
}

int64_t AVSyncClock::getAPts() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!isValid() || isPaused_.load()) return aLastPts_;

    // 理论PTS
    int64_t basePts = aFrameCount_ * static_cast<int64_t>(round(aFrameDurUs_));
    // 叠加当前帧的补偿量
    int64_t compensateUs = static_cast<int64_t>(aCompensatePerFrameUs_);
    int64_t newPts = basePts + compensateUs;

    if (newPts <= aLastPts_) {
        newPts = aLastPts_ + 1;
    }

    // 更新剩余补偿量）
    if (aTotalCompensateUs_ != 0) {
        aTotalCompensateUs_ -= compensateUs;
        // 若剩余补偿量≤0或已到分摊帧数，停止补偿
        if (abs(aTotalCompensateUs_) <= abs(compensateUs) || aFrameCount_ % 30 == 0) {
            aTotalCompensateUs_ = 0;
            aCompensatePerFrameUs_ = 0;
            // qDebug() << "音频渐进校准完成：剩余偏差" << aTotalCompensateUs_ << "微秒";
        }
    }
    aLastPts_ = newPts;
    aFrameCount_++;

    return newPts;
}

// 获取当前视频PTS（自动递增并校准）
int64_t AVSyncClock::getVPts() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!isValid() || isPaused_.load()) return vPts_;

    vPts_ = vFrameCount_ * static_cast<int64_t>(round(vFrameDurUs_));
    // qDebug() << "[时钟帧计数] 当前计数:" << vFrameCount_ << "PTS:" << vPts_; // 新增日志
    ++vFrameCount_;
    return vPts_;
}

// void AVSyncClock::calibrateAudio(int64_t externalTsUs) { // externalTsUs是系统流逝时间
//     std::lock_guard<std::mutex> lock(mtx_);
//     if (!isValid() || externalTsUs <= 0  || isPaused_.load()) return;

//     // 1. 计算理论目标PTS（系统流逝时间对应的PTS）
//     int64_t targetPts = externalTsUs - pauseDuration_;
//     // 2. 计算当前实际PTS（基于帧计数）
//     int64_t currentPts = aFrameCount_ * static_cast<int64_t>(round(aFrameDurUs_));
//     // 3. 计算总偏差（需补偿的微秒数）
//     int64_t diffUs = targetPts - currentPts;

//     // 若偏差≤1帧时长（21333微秒）
//     if (abs(diffUs) <= static_cast<int64_t>(aFrameDurUs_ * 0.5)) {
//         aTotalCompensateUs_ = 0;
//         aCompensatePerFrameUs_ = 0;
//         return;
//     }

//     // 4. 若偏差>1帧，计算分散补偿方案：用接下来的30帧分摊（可调整）
//     const int compensateFrames = 30; // 30帧内分摊完偏差
//     aTotalCompensateUs_ = diffUs;
//     // 每帧补偿量=总偏差÷分摊帧数（取整，避免浮点数误差）
//     aCompensatePerFrameUs_ = static_cast<double>(diffUs) / compensateFrames;

//     qDebug() << "音频渐进校准：总偏差" << diffUs << "微秒，将在" << compensateFrames
//              << "帧内分摊，每帧补偿" << aCompensatePerFrameUs_ << "微秒";
// }

void AVSyncClock::calibrateAudio(int64_t externalTsUs) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!isValid() || externalTsUs <= 0  || isPaused_.load()) return;
    if (externalTsUs <= aLastExternalTs_) return;
    aLastExternalTs_ = externalTsUs;


    int64_t targetPts = externalTsUs - pauseDuration_ - audioOffset;
    int64_t currentPts = aFrameCount_ * static_cast<int64_t>(round(aFrameDurUs_));
    int64_t diffUs = targetPts - currentPts;

    // 1. 微小偏差直接忽略
    if (abs(diffUs) <= static_cast<int64_t>(aFrameDurUs_ * 0.5)) {
        aTotalCompensateUs_ = 0;
        aCompensatePerFrameUs_ = 0;
        return;
    }

    // 2. 计算安全的单帧补偿量（≤2倍帧时长）
    int64_t maxPerFrameComp = static_cast<int64_t>(aFrameDurUs_ * 2);
    // 3. 动态计算所需分摊帧数（确保单帧补偿不超标）
    int compensateFrames = std::max(30, (int)(abs(diffUs) / maxPerFrameComp) + 1);
    aCompensatePerFrameUs_ = static_cast<double>(diffUs) / compensateFrames;

    // 4. 最终保险：强制限制在安全范围内
    if (abs(aCompensatePerFrameUs_) > maxPerFrameComp) {
        aCompensatePerFrameUs_ = (diffUs > 0) ? maxPerFrameComp : -maxPerFrameComp;
    }

    aTotalCompensateUs_ = diffUs;
    // qDebug() << "音频校准：总偏差" << diffUs << "微秒，分" << compensateFrames
    //          << "帧分摊，每帧补偿" << aCompensatePerFrameUs_ << "微秒（安全范围）externalTsUs:" << externalTsUs;
}

// 用外部视频时间戳校准（如图层的物理时钟）
void AVSyncClock::calibrateVideo(int64_t externalTsUs) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!isValid() || externalTsUs <= 0  || isPaused_.load()) return;
    if (externalTsUs <= vLastExternalTs_) return;
    vLastExternalTs_ = externalTsUs;

    int64_t expectPts = vFrameCount_ * static_cast<int64_t>(round(vFrameDurUs_));
    int64_t actualPts = (externalTsUs - startUs_) - pauseDuration_;
    int64_t diff = expectPts - actualPts;
    if (std::abs(diff) > maxAllowedDiffUs_) {
        // 直接把计数器修正到外部时间，后续继续匀速
        vFrameCount_ = actualPts / vFrameDurUs_;
        vPts_        = vFrameCount_ * static_cast<int64_t>(round(vFrameDurUs_));
        qDebug() << "视频低频漂移校正：期望" << expectPts
                 << "实际" << actualPts
                 << "→重置帧号=" << vFrameCount_;
    }
}

void AVSyncClock::setAPts(int64_t pts)
{
    std::lock_guard<std::mutex> lock(mtx_);
    aPts_ = pts;
    aLastPts_ = pts;
}
