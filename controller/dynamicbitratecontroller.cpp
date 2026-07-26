#include "dynamicbitratecontroller.h"

#include "encoder/vencoder.h"
#include "queue/packetqueue.h"

#include <QDateTime>
#include <QDebug>
#include <algorithm>

DynamicBitrateController::DynamicBitrateController(
    PacketQueue* vPktQueue,
    PacketQueue* aPktQueue,
    VEncoder* vEncoder,
    AEncoder* aEncoder,
    NetMonitor* netMonitor,
    QObject* parent)
    : QObject(parent)
    , vPktQueue_(vPktQueue)
    , aPktQueue_(aPktQueue)
    , vEncoder_(vEncoder)
    , aEncoder_(aEncoder)
    , netMonitor_(netMonitor)
{
    Q_UNUSED(aEncoder_);
    if (netMonitor_) {
        connect(netMonitor_, &NetMonitor::statsUpdated,
                this, &DynamicBitrateController::onStatsUpdated,
                Qt::QueuedConnection);
    }
    monitorTimer_.setInterval(MonitorIntervalMs);
    connect(&monitorTimer_, &QTimer::timeout,
            this, &DynamicBitrateController::onMonitorTimerTimeout);
}

void DynamicBitrateController::init(int initVBitrate, int initABitrate,
                                    int minVBitrateRatio)
{
    QMutexLocker locker(&mutex_);
    initVideoBitrate_ = initVBitrate > 0 ? initVBitrate : 2000000;
    initAudioBitrate_ = initABitrate > 0 ? initABitrate : 128000;
    const int safeRatio = std::clamp(minVBitrateRatio, 10, 100);
    minVideoBitrate_ = std::min(initVideoBitrate_,
                                std::max(initVideoBitrate_ * safeRatio / 100, 300000));
    currentVideoBitrate_ = initVideoBitrate_;
    congestedSamples_ = 0;
    recoveredSamples_ = 0;
    previousDroppedFrames_ = 0;
    previousVideoQueueDelayMs_ = 0;
    previousBytesInflight_ = 0;
    latestStatsTimestamp_ = 0;
    lastAdjustTimestamp_ = 0;
    adjustState_ = BitrateAdjustState::Stable;
    qInfo().nospace() << "[ABR] init video=" << initVideoBitrate_ / 1000
                      << "kbps audio=" << initAudioBitrate_ / 1000
                      << "kbps minVideo=" << minVideoBitrate_ / 1000 << "kbps";
}

void DynamicBitrateController::start()
{
    if (!monitorTimer_.isActive()) {
        if (netMonitor_) onStatsUpdated(netMonitor_->getCurrentStats());
        monitorTimer_.start();
        qInfo() << "[ABR] monitoring started, interval=" << MonitorIntervalMs << "ms";
    }
}

void DynamicBitrateController::stop()
{
    const bool wasActive = monitorTimer_.isActive();
    monitorTimer_.stop();
    QMutexLocker locker(&mutex_);
    congestedSamples_ = 0;
    recoveredSamples_ = 0;
    latestStatsTimestamp_ = 0;
    adjustState_ = BitrateAdjustState::Stable;
    if (wasActive) qInfo() << "[ABR] monitoring stopped";
}

int DynamicBitrateController::getCurrentVideoBitrate() const
{
    QMutexLocker locker(&mutex_);
    return currentVideoBitrate_;
}

BitrateAdjustState DynamicBitrateController::getAdjustState() const
{
    QMutexLocker locker(&mutex_);
    return adjustState_;
}

void DynamicBitrateController::onStatsUpdated(const NetworkStats& stats)
{
    QMutexLocker locker(&mutex_);
    latestStats_ = stats;
    latestStatsTimestamp_ = QDateTime::currentMSecsSinceEpoch();
}

bool DynamicBitrateController::isCongested(const NetworkStats& stats) const
{
    if (!stats.isStreaming()) return false;

    const bool dropped = stats.droppedVideoFrames > previousDroppedFrames_;
    const bool queueHigh = stats.videoQueueDelayMs >= 350 ||
                           stats.audioQueueDelayMs >= 500;
    const bool queueRising = stats.videoQueueDelayMs >= 180 &&
                             stats.videoQueueDelayMs - previousVideoQueueDelayMs_ >= 60;
    const bool writeBlocked = stats.socketWriteBlockMs >= 150;
    const bool throughputDeficit = stats.videoQueueDelayMs >= 150 &&
                                   stats.encodeInputBps > 0 &&
                                   stats.sendThroughputBps >= 0 &&
                                   stats.sendThroughputBps * 100 < stats.encodeInputBps * 92;
    const qint64 inflightIncrease = stats.bytesInflight - previousBytesInflight_;
    const bool transportBacklogGrowing = stats.videoQueueDelayMs >= 150 &&
                                         inflightIncrease > 256 * 1024;
    const bool latencyCorroborates = stats.videoQueueDelayMs >= 150 &&
                                     stats.rttMs >= 350;
    const bool localQueuePressure = (vPktQueue_ && vPktQueue_->size() > 20) ||
                                    (aPktQueue_ && aPktQueue_->size() > 35);

    // 无队列场景下的拥塞判断（队列被 backpressure 清空后 vq=0ms 但网络已阻塞）
    const bool sendStalled = stats.encodeInputBps > 500000 &&
                             stats.sendThroughputBps < 100000;
    const bool massiveInflight = stats.bytesInflight > 10 * 1024 * 1024;
    const bool highRttWithDrops = stats.rttMs >= 300 && dropped;

    return dropped || queueHigh || queueRising || writeBlocked ||
           throughputDeficit || transportBacklogGrowing ||
           latencyCorroborates || localQueuePressure ||
           sendStalled || massiveInflight || highRttWithDrops;
}

bool DynamicBitrateController::isRecovered(const NetworkStats& stats) const
{
    if (!stats.isStreaming()) return false;
    const bool noNewDrops = stats.droppedVideoFrames == previousDroppedFrames_;
    const bool queuesHealthy = stats.videoQueueDelayMs < 100 &&
                               stats.audioQueueDelayMs < 150 &&
                               stats.videoQueueDelayMs <= previousVideoQueueDelayMs_ + 20 &&
                               (!vPktQueue_ || vPktQueue_->size() <= 5) &&
                               (!aPktQueue_ || aPktQueue_->size() <= 8);
    const bool transportHealthy = stats.socketWriteBlockMs < 50 &&
                                  (stats.rttMs <= 0 || stats.rttMs < 250);
    // 无队列场景下的恢复判断：inflight 回落且发送恢复
    const bool inflightCleared = stats.bytesInflight < 2 * 1024 * 1024;
    const bool sendResumed = stats.sendThroughputBps > stats.encodeInputBps * 0.8;
    return noNewDrops && queuesHealthy && transportHealthy &&
           inflightCleared && sendResumed;
}

int DynamicBitrateController::congestionTarget(const NetworkStats& stats) const
{
    double decreaseFactor = 0.82;
    if (stats.videoQueueDelayMs >= 1000 ||
        stats.socketWriteBlockMs >= 500 ||
        stats.droppedVideoFrames > previousDroppedFrames_ ||
        stats.bytesInflight > 20 * 1024 * 1024) {
        decreaseFactor = 0.68;
    } else if (stats.videoQueueDelayMs >= 500 || stats.socketWriteBlockMs >= 250 ||
               stats.bytesInflight > 10 * 1024 * 1024) {
        decreaseFactor = 0.75;
    }

    int target = static_cast<int>(currentVideoBitrate_ * decreaseFactor);
    if (stats.sendThroughputBps > initAudioBitrate_ + minVideoBitrate_) {
        const int sustainableVideo = static_cast<int>(
            (stats.sendThroughputBps - initAudioBitrate_) * 0.90);
        target = std::min(target, sustainableVideo);
    }
    return std::clamp(target, minVideoBitrate_, initVideoBitrate_);
}

bool DynamicBitrateController::applyVideoBitrate(int targetBitrate)
{
    if (!vEncoder_ || !vEncoder_->getCodecContext() || targetBitrate <= 0) return false;
    vEncoder_->setBitrate(targetBitrate);
    return true;
}

void DynamicBitrateController::onMonitorTimerTimeout()
{
    NetworkStats stats;
    int oldBitrate = 0;
    int targetBitrate = 0;
    BitrateAdjustState nextState = BitrateAdjustState::Stable;

    {
        QMutexLocker locker(&mutex_);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        stats = latestStats_;
        if (!stats.isStreaming() || latestStatsTimestamp_ <= 0 ||
            now - latestStatsTimestamp_ > StatsStaleTimeoutMs) {
            congestedSamples_ = 0;
            recoveredSamples_ = 0;
            adjustState_ = BitrateAdjustState::Stable;
            // 注意：不在这里更新 previousDroppedFrames_，避免掩盖累计丢帧
            previousVideoQueueDelayMs_ = stats.videoQueueDelayMs;
            previousBytesInflight_ = stats.bytesInflight;
            return;
        }

        if (isCongested(stats)) {
            ++congestedSamples_;
            recoveredSamples_ = 0;
        } else if (isRecovered(stats)) {
            ++recoveredSamples_;
            congestedSamples_ = 0;
        } else {
            congestedSamples_ = 0;
            recoveredSamples_ = 0;
        }

        const qint64 sinceAdjust = now - lastAdjustTimestamp_;
        oldBitrate = currentVideoBitrate_;
        if (congestedSamples_ >= CongestedSamplesRequired &&
            sinceAdjust >= LowerAdjustIntervalMs) {
            targetBitrate = congestionTarget(stats);
            nextState = BitrateAdjustState::Lowering;
        } else if (recoveredSamples_ >= RecoveredSamplesRequired &&
                   currentVideoBitrate_ < initVideoBitrate_ &&
                   sinceAdjust >= RaiseAdjustIntervalMs) {
            const int additiveStep = std::max(50000, initVideoBitrate_ / 20);
            targetBitrate = std::min(initVideoBitrate_,
                                     currentVideoBitrate_ + additiveStep);
            nextState = BitrateAdjustState::Raising;
        } else {
            adjustState_ = BitrateAdjustState::Stable;
        }

        // Bug fix: previousDroppedFrames_ 只在每次采样后更新，不在调整后重置
        // 这样 dropped 判断能持续感知累计丢帧趋势
        previousDroppedFrames_ = stats.droppedVideoFrames;
        previousVideoQueueDelayMs_ = stats.videoQueueDelayMs;
        previousBytesInflight_ = stats.bytesInflight;
    }

    if (targetBitrate <= 0 || targetBitrate == oldBitrate) return;
    if (!applyVideoBitrate(targetBitrate)) {
        qWarning() << "[ABR] failed to apply bitrate" << targetBitrate / 1000 << "kbps";
        return;
    }

    {
        QMutexLocker locker(&mutex_);
        currentVideoBitrate_ = targetBitrate;
        adjustState_ = nextState;
        lastAdjustTimestamp_ = QDateTime::currentMSecsSinceEpoch();
        congestedSamples_ = 0;
        recoveredSamples_ = 0;
    }

    const char* action = nextState == BitrateAdjustState::Lowering ? "lower" : "raise";
    qInfo().nospace()
        << "[ABR] " << action
        << " bitrate=" << oldBitrate / 1000 << "->" << targetBitrate / 1000 << "kbps"
        << " send=" << stats.sendThroughputBps / 1000 << "kbps"
        << " input=" << stats.encodeInputBps / 1000 << "kbps"
        << " inflight=" << stats.bytesInflight
        << " vq=" << stats.videoQueueDelayMs << "ms"
        << " aq=" << stats.audioQueueDelayMs << "ms"
        << " writeBlock=" << stats.socketWriteBlockMs << "ms"
        << " rtt=" << stats.rttMs << "ms"
        << " jitter=" << stats.jitterMs << "ms"
        << " dropped=" << stats.droppedVideoFrames
        << " videoPackets=" << (vPktQueue_ ? vPktQueue_->size() : 0)
        << " audioPackets=" << (aPktQueue_ ? aPktQueue_->size() : 0);
    emit bitrateAdjusted(oldBitrate, targetBitrate, nextState);
}
