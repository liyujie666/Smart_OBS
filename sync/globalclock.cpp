// #include "globalclock.h"
// #include <cmath>
// #include <QDebug>


// void GlobalClock::initAudio(int sampleRate, int nbSamples) {
//     aDur_ = (1000000.0 * nbSamples) / sampleRate; // 如44100Hz+1024样本 → ~23219us
// }


// void GlobalClock::initVideo(int fps) {
//     vDur_ = 1000000.0 / fps; // 如30fps → ~33333us
// }

// void GlobalClock::start(ClkMode mode) {
//     mode_ = mode;
//     startUs_ = getCurrentUs();
//     aPts_ = 0;
//     vPts_ = 0;
//     lastAPts_ = -1;
//     lastVPts_ = -1;
//     aInitialized = false;
// }

// int64_t GlobalClock::getAPts() {
//     if (!isValid()) return 0;

//     int64_t currentUs = getCurrentUs();
//     int64_t frameDurationUs = aDur_; // 21333μs（48000Hz+1024样本）

//     if (lastAPts_ < 0) {
//         lastAPts_ = startUs_; // 第一帧PTS = 时钟启动时间
//         aPts_ = lastAPts_;
//         lastAFrameTimeUs_ = currentUs; // 记录第一帧的实际生成时间
//         return aPts_;
//     }

//     // 3. 计算理论PTS：上一帧PTS + 帧时长
//     int64_t idealPts = lastAPts_ + frameDurationUs;

//     int64_t actualIntervalUs = currentUs - lastAFrameTimeUs_; // 实际间隔（含编码耗时）
//     int64_t offsetUs = actualIntervalUs - frameDurationUs; // 实际与理论的偏差
//     const int64_t MAX_OFFSET = frameDurationUs * 0.5; // 最大允许偏差（10.6ms）

//     if (std::abs(offsetUs) > MAX_OFFSET) {
//         // 渐进修正：每次修正偏差的20%，防止PTS跳变
//         idealPts += offsetUs * 0.2;
//         qDebug() << "音频PTS微调：" << offsetUs * 0.2 << "μs（偏差：" << offsetUs << "μs）";
//     }


//     // 6. 确保PTS严格递增
//     if (idealPts <= lastAPts_) {
//         idealPts = lastAPts_ + 1; // 至少比上一帧大1μs
//         qDebug() << "修复PTS倒序：" << lastAPts_ << "→" << idealPts;
//     }

//     // 7. 更新状态变量
//     aPts_ = idealPts;
//     lastAPts_ = aPts_;
//     lastAFrameTimeUs_ = currentUs;

//     // qDebug() << "getAPts 视频PTS:" << vPts_ << "音频PTS:" << aPts_ << "偏差(视-音):" << vPts_ - aPts_;
//     return aPts_;
// }


// int64_t GlobalClock::getVPts() {
//     if (!isValid()) return 0;

//     int64_t currentUs = getCurrentUs();
//     int64_t frameDurationUs = vDur_; // 例如 60fps 时约 16667μs

//     if (lastVPts_ < 0) {
//         lastVPts_ = startUs_; // 第一帧 PTS = 时钟启动时间
//         vPts_ = lastVPts_;
//         lastVFrameTimeUs_ = currentUs; // 记录第一帧实际生成时间
//         return vPts_;
//     }

//     int64_t idealPts = lastVPts_ + frameDurationUs;

//     int64_t actualIntervalUs = currentUs - lastVFrameTimeUs_; // 实际帧间隔（含编码耗时）
//     int64_t offsetUs = actualIntervalUs - frameDurationUs; // 偏差量
//     const int64_t MAX_OFFSET = frameDurationUs * 0.1; // 最大允许偏差（如 60fps 时 8333μs）

//     if (std::abs (offsetUs) > MAX_OFFSET) {
//         // 渐进修正：每次矫正 20% 偏差，避免 PTS 跳变
//         idealPts += offsetUs * 0.2;
//         qDebug () << "视频 PTS 微调：" << offsetUs * 0.2 << "μs（偏差：" << offsetUs << "μs）";
//     }

//     if (mode_ == ClkMode::AMaster) {
//         int64_t syncDiff = vPts_ - aPts_; // 视频PTS与音频PTS的偏差
//         const int64_t MAX_SYNC_DIFF = 50000; // 最大允许偏差（50ms）
//         if (std::abs(syncDiff) > MAX_SYNC_DIFF) {
//             // 视频PTS向音频PTS对齐（修正50%偏差）
//             idealPts = vPts_ - syncDiff * 0.5;
//             qDebug() << "视频PTS同步校准（音频为主）：修正" << syncDiff * 0.5 << "μs";
//         }
//     }
//     if (idealPts <= lastVPts_) {
//         idealPts = lastVPts_ + 1; // 至少比上一帧大 1μs
//         qDebug () << "修复视频 PTS 倒序：" << lastVPts_ << "→" << idealPts;
//     }

//     // 7. 更新状态变量
//     vPts_ = idealPts;
//     lastVPts_ = vPts_;
//     lastVFrameTimeUs_ = currentUs;

//     // qDebug() << "getVPts 视频PTS:" << vPts_ << "音频PTS:" << aPts_ << "偏差(视-音):" << vPts_ - aPts_;

//     return vPts_;
// }
