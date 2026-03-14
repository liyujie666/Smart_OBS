#include "dynamicbitratecontroller.h"
#include "queue/packetqueue.h"
#include "encoder/vencoder.h"
#include "encoder/aencoder.h"
#include <QDebug>
#include <QDateTime>
#include <climits>

DynamicBitrateController::DynamicBitrateController(
    PacketQueue* vPktQueue,
    PacketQueue* aPktQueue,
    VEncoder* vEncoder,
    AEncoder* aEncoder,
    NetworkMonitor* networkMonitor,
    QObject *parent)
    : QObject(parent)
    , vPktQueue_(vPktQueue)
    , aPktQueue_(aPktQueue)
    , vEncoder_(vEncoder)
    , aEncoder_(aEncoder)
    , networkMonitor_(networkMonitor)
{
    {
        QMutexLocker locker(&networkStateMutex_);
        currentLossRate_ = 0.0f;
        currentDelayMs_ = -1;
        currentPortStatus_ = -1;
        currentStreamBitrate_ = -1;
    }

    if (networkMonitor_) {
        connect(networkMonitor_, &NetworkMonitor::monitorRealTimeResult,
                this, &DynamicBitrateController::onNetworkRealTimeResult);
        connect(networkMonitor_, &NetworkMonitor::monitorFinalResult,
                this, &DynamicBitrateController::onNetworkFinalResult);
    }

    monitorTimer_.setInterval(MONITOR_INTERVAL_);
    monitorTimer_.setSingleShot(false);
    connect(&monitorTimer_, &QTimer::timeout, this, &DynamicBitrateController::onMonitorTimerTimeout);
}

void DynamicBitrateController::init(int initVBitrate, int initABitrate, int minVBitrateRatio)
{
    QMutexLocker locker(&networkStateMutex_);

    if (initVBitrate <= 0) {
        qWarning() << "DynamicBitrateController init: invalid initVBitrate, fallback to 2000Kbps";
        initVBitrate = 2000 * 1000;
    }
    if (initABitrate <= 0) {
        initABitrate = 128 * 1000;
    }

    initVideoBitrate_ = initVBitrate;
    initAudioBitrate_ = initABitrate;
    minVideoBitrate_ = std::max(initVBitrate * minVBitrateRatio / 100, 500 * 1000);
    currentVideoBitrate_ = initVideoBitrate_;
    currentAudioBitrate_ = initAudioBitrate_;

    qDebug() << "DBC init: video" << initVideoBitrate_ / 1000 << "Kbps,"
             << "min" << minVideoBitrate_ / 1000 << "Kbps,"
             << "audio" << initAudioBitrate_ / 1000 << "Kbps";
}

void DynamicBitrateController::start()
{
    if (!monitorTimer_.isActive()) {
        monitorTimer_.start();
        qDebug() << "DynamicBitrateController started, interval =" << MONITOR_INTERVAL_ << "ms";
    }
}

void DynamicBitrateController::stop()
{
    if (monitorTimer_.isActive()) {
        monitorTimer_.stop();
        qDebug() << "DynamicBitrateController stopped";
    }
}

int DynamicBitrateController::getCurrentVideoBitrate() const
{
    QMutexLocker locker(&networkStateMutex_);
    return currentVideoBitrate_;
}

BitrateAdjustState DynamicBitrateController::getAdjustState() const
{
    QMutexLocker locker(&networkStateMutex_);
    return adjustState_;
}

void DynamicBitrateController::onMonitorTimerTimeout()
{
    QMutexLocker locker(&networkStateMutex_);

    if (!vEncoder_ || !vEncoder_->getCodecContext()) {
        qWarning() << "DynamicBitrateController: video encoder not initialized, skip adjustment";
        return;
    }
    if (!vPktQueue_ || !aPktQueue_) {
        qWarning() << "DynamicBitrateController: packet queues not initialized, skip adjustment";
        return;
    }

    qint64 currentTimestamp = QDateTime::currentMSecsSinceEpoch();
    qint64 intervalSinceLastAdjust = currentTimestamp - lastAdjustTimestamp_;
    if (intervalSinceLastAdjust < 0) {
        lastAdjustTimestamp_ = currentTimestamp;
        return;
    }

    int targetBitrate = currentVideoBitrate_;
    bool needAdjust = false;

    if (isNetworkCongested() && intervalSinceLastAdjust >= ADJUST_INTERVAL_LOWER_) {
        targetBitrate = calculateTargetVideoBitrate();
        needAdjust = (targetBitrate < currentVideoBitrate_);
    } else if (isNetworkRecovered() && intervalSinceLastAdjust >= ADJUST_INTERVAL_RAISE_ && currentVideoBitrate_ < initVideoBitrate_) {
        targetBitrate = calculateTargetVideoBitrate();
        needAdjust = (targetBitrate > currentVideoBitrate_);
    }

    if (needAdjust && targetBitrate != currentVideoBitrate_) {
        int oldVideoBitrate = currentVideoBitrate_;
        if (updateVideoEncoderBitrate(targetBitrate)) {
            currentVideoBitrate_ = targetBitrate;
            if (aEncoder_) {
                updateAudioEncoderBitrate(targetBitrate);
            }
            adjustState_ = (targetBitrate > oldVideoBitrate) ? BitrateAdjustState::Raising : BitrateAdjustState::Lowering;
            lastAdjustTimestamp_ = currentTimestamp;
            emit bitrateAdjusted(oldVideoBitrate, targetBitrate, adjustState_);
            qDebug() << "Bitrate adjusted from" << oldVideoBitrate/1000 << "Kbps to" << targetBitrate/1000 << "Kbps";
        } else {
            qWarning() << "Failed to apply target bitrate" << targetBitrate/1000 << "Kbps";
        }
    } else if (adjustState_ != BitrateAdjustState::Stable) {
        adjustState_ = BitrateAdjustState::Stable;
    }
}

void DynamicBitrateController::onNetworkRealTimeResult(const NetworkMonitorResult& result)
{
    QMutexLocker locker(&networkStateMutex_);

    switch (result.type) {
    case MonitorType::ZLMEDIAKIT:
        if (result.isSuccess) {
            currentLossRate_ = result.lossRate;
            currentDelayMs_ = result.delayMs;
            currentStreamBitrate_ = result.streamBitrateKbps;
        }
        break;
    case MonitorType::TCP_CONNECT:
        currentPortStatus_ = result.portStatus;
        if (result.isSuccess) {
            currentDelayMs_ = result.delayMs;
        }
        break;
    case MonitorType::UDP_PING:
        if (result.isSuccess) {
            currentLossRate_ = result.lossRate;
            currentDelayMs_ = result.delayMs;
        }
        break;
    }
}

void DynamicBitrateController::onNetworkFinalResult(const NetworkMonitorResult& result)
{
    QMutexLocker locker(&networkStateMutex_);

    if (result.type == MonitorType::TCP_CONNECT) {
        currentPortStatus_ = result.portStatus;
        currentDelayMs_ = result.delayMs;
    } else if (result.type == MonitorType::UDP_PING) {
        currentLossRate_ = result.lossRate;
        currentDelayMs_ = result.delayMs;
    }
}

bool DynamicBitrateController::isNetworkCongested() const
{
    int theoreticalBandwidth = -1;
    if (currentVideoBitrate_ > 0 && currentAudioBitrate_ > 0) {
        theoreticalBandwidth = (currentVideoBitrate_ / 1000) ;
    }

    bool bandwidthInsufficient = false;
    if (currentStreamBitrate_ != -1 && theoreticalBandwidth != -1) {
        float bandwidthRatio = static_cast<float>(currentStreamBitrate_) / theoreticalBandwidth;
        bandwidthInsufficient = (bandwidthRatio < BITRATE_BANDWIDTH_RATIO_THRESHOLD_);
    }

    bool tcpClosed = (currentPortStatus_ == 0);
    bool videoQueueFull = (vPktQueue_ && vPktQueue_->size() > VIDEO_QUEUE_THRESHOLD_);
    bool audioQueueFull = (aPktQueue_ && aPktQueue_->size() > AUDIO_QUEUE_THRESHOLD_);
    bool highLoss = (currentLossRate_ > 3.0f && currentLossRate_ != -1.0f);
    bool highDelay = (currentDelayMs_ > 300 && currentDelayMs_ != -1);

    bool isCongested = tcpClosed || videoQueueFull || audioQueueFull || highLoss || highDelay || bandwidthInsufficient;

    if (isCongested) {
        qDebug() << "Network congested:"
                 << "tcpClosed=" << tcpClosed
                 << "vQueue=" << (vPktQueue_ ? vPktQueue_->size() : 0)
                 << "aQueue=" << (aPktQueue_ ? aPktQueue_->size() : 0)
                 << "loss=" << currentLossRate_
                 << "delay=" << currentDelayMs_
                 << "remoteKbps=" << currentStreamBitrate_;
    }

    return isCongested;
}

bool DynamicBitrateController::isNetworkRecovered() const
{
    bool tcpOpen = (currentPortStatus_ == 1 || currentPortStatus_ == -1);
    bool videoQueueEmpty = (vPktQueue_ && vPktQueue_->size() <= QUEUE_EMPTY_THRESHOLD_);
    bool audioQueueEmpty = (aPktQueue_ && aPktQueue_->size() <= QUEUE_EMPTY_THRESHOLD_);
    bool lowLoss = (currentLossRate_ < 1.0f || currentLossRate_ == -1.0f);
    bool lowDelay = (currentDelayMs_ < 150 || currentDelayMs_ == -1);

    return tcpOpen && videoQueueEmpty && audioQueueEmpty && lowLoss && lowDelay;
}

int DynamicBitrateController::calculateTargetVideoBitrate()
{
    int targetBitrate = currentVideoBitrate_;

    if (isNetworkCongested()) {
        targetBitrate = static_cast<int>(currentVideoBitrate_ * 0.9);
        targetBitrate = std::max(targetBitrate, minVideoBitrate_);
    } else if (isNetworkRecovered() && currentVideoBitrate_ < initVideoBitrate_) {
        targetBitrate = static_cast<int>(currentVideoBitrate_ * 1.05);
        targetBitrate = std::min(targetBitrate, initVideoBitrate_);
    }

    return targetBitrate;
}

bool DynamicBitrateController::updateVideoEncoderBitrate(int targetBitrate)
{
    if (!vEncoder_ || !vEncoder_->getCodecContext()) {
        qWarning() << "DynamicBitrateController: invalid video encoder";
        return false;
    }

    AVCodecContext* vCodecCtx = vEncoder_->getCodecContext();
    const char* codecName = vCodecCtx->codec->name;

    if (strstr(codecName, "nvenc")) {
        av_opt_set_int(vCodecCtx->priv_data, "max_bitrate", targetBitrate, 0);
        av_opt_set_int(vCodecCtx->priv_data, "min_bitrate", targetBitrate, 0);
        av_opt_set_int(vCodecCtx, "bit_rate", targetBitrate, 0);
    } else if (strcmp(codecName, "libx264") == 0 || strcmp(codecName, "libx265") == 0) {
        vCodecCtx->bit_rate = targetBitrate;
        vCodecCtx->bit_rate_tolerance = targetBitrate / 4;
    } else {
        av_opt_set_int(vCodecCtx, "bit_rate", targetBitrate, 0);
    }

    return true;
}

bool DynamicBitrateController::updateAudioEncoderBitrate(int targetVideoBitrate)
{
    if (!aEncoder_ || !aEncoder_->getCodecContext() || initVideoBitrate_ == 0) {
        qWarning() << "DynamicBitrateController: audio encoder invalid or init video bitrate not set";
        return false;
    }

    float adjustRatio = static_cast<float>(targetVideoBitrate) / initVideoBitrate_;
    int targetAudioBitrate = static_cast<int>(initAudioBitrate_ * adjustRatio);
    targetAudioBitrate = std::clamp(targetAudioBitrate, 64 * 1000, 320 * 1000);

    AVCodecContext* aCodecCtx = aEncoder_->getCodecContext();
    av_opt_set_int(aCodecCtx, "bit_rate", targetAudioBitrate, 0);
    currentAudioBitrate_ = targetAudioBitrate;

    return true;
}
