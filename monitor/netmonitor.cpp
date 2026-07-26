#include "netmonitor.h"

#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>
#include <cmath>

NetMonitor* NetMonitor::instance()
{
    static NetMonitor monitor;
    return &monitor;
}

NetMonitor::NetMonitor(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<NetworkStats>("NetworkStats");
    qRegisterMetaType<rtmp::SessionState>("rtmp::SessionState");
}

void NetMonitor::updateRtmpStats(const rtmp::RtmpStats& source)
{
    NetworkStats snapshot;
    bool shouldLog = false;
    bool levelChanged = false;
    {
        QMutexLocker locker(&mutex_);
        stats_.bytesSent = source.bytesSent;
        stats_.bytesAcked = source.bytesAcked;
        stats_.bytesInflight = source.bytesInflight;
        stats_.sendThroughputBps = source.sendThroughputBps;
        stats_.encodeInputBps = source.encodeInputBps;
        stats_.rttMs = source.rttMs;
        stats_.videoQueueDelayMs = source.videoQueueDelayMs;
        stats_.audioQueueDelayMs = source.audioQueueDelayMs;
        stats_.socketWriteBlockMs = source.socketWriteBlockMs;
        stats_.droppedVideoFrames = source.droppedVideoFrames;
        stats_.requestedKeyframes = source.requestedKeyframes;
        stats_.sessionState = source.state;

        if (source.rttMs > 0) {
            rttHistory_.append(source.rttMs);
            if (rttHistory_.size() > 10) rttHistory_.pop_front();
        }
        stats_.jitterMs = calculateJitterLocked();
        stats_.networkLevel = calculateNetworkLevelLocked();
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        levelChanged = stats_.networkLevel != lastLoggedNetworkLevel_;
        shouldLog = levelChanged || now - lastStatsLogMs_ >= 3000;
        if (shouldLog) {
            lastStatsLogMs_ = now;
            lastLoggedNetworkLevel_ = stats_.networkLevel;
        }
        snapshot = stats_;
    }
    if (shouldLog) {
        qInfo().nospace()
            << "[NetMonitor] level=" << snapshot.networkLevel
            << (levelChanged ? "(changed)" : "")
            << " state=" << rtmp::toString(snapshot.sessionState)
            << " send=" << snapshot.sendThroughputBps / 1000 << "kbps"
            << " input=" << snapshot.encodeInputBps / 1000 << "kbps"
            << " sent=" << snapshot.bytesSent
            << " acked=" << snapshot.bytesAcked
            << " inflight=" << snapshot.bytesInflight
            << " rtt=" << snapshot.rttMs << "ms"
            << " jitter=" << snapshot.jitterMs << "ms"
            << " vq=" << snapshot.videoQueueDelayMs << "ms"
            << " aq=" << snapshot.audioQueueDelayMs << "ms"
            << " writeBlock=" << snapshot.socketWriteBlockMs << "ms"
            << " dropped=" << snapshot.droppedVideoFrames
            << " reconnects=" << snapshot.reconnectCount;
    }
    emit statsUpdated(snapshot);
}

void NetMonitor::updateSessionState(rtmp::SessionState state, const QString& detail)
{
    NetworkStats snapshot;
    rtmp::SessionState oldState = rtmp::SessionState::Idle;
    {
        QMutexLocker locker(&mutex_);
        oldState = previousState_;
        if (state == rtmp::SessionState::Streaming) {
            hasStreamed_ = true;
            reconnectPending_ = false;
        } else if (hasStreamed_ &&
                   (state == rtmp::SessionState::Error ||
                    state == rtmp::SessionState::Backoff ||
                    state == rtmp::SessionState::Reconnecting)) {
            reconnectPending_ = true;
        } else if (reconnectPending_ && state == rtmp::SessionState::Connecting) {
            ++stats_.reconnectCount;
            reconnectPending_ = false;
        }
        previousState_ = state;
        stats_.sessionState = state;
        stats_.stateDetail = detail;
        stats_.networkLevel = calculateNetworkLevelLocked();
        snapshot = stats_;
    }
    qInfo().nospace() << "[NetMonitor] session "
                      << rtmp::toString(oldState) << " -> "
                      << rtmp::toString(state)
                      << " detail=" << (detail.isEmpty() ? QStringLiteral("-") : detail)
                      << " reconnects=" << snapshot.reconnectCount;
    emit sessionStateChanged(state, detail);
    emit statsUpdated(snapshot);
}

NetworkStats NetMonitor::getCurrentStats() const
{
    QMutexLocker locker(&mutex_);
    return stats_;
}

void NetMonitor::reset()
{
    NetworkStats snapshot;
    {
        QMutexLocker locker(&mutex_);
        stats_ = NetworkStats{};
        previousState_ = rtmp::SessionState::Idle;
        hasStreamed_ = false;
        reconnectPending_ = false;
        lastStatsLogMs_ = 0;
        lastLoggedNetworkLevel_ = -1;
        rttHistory_.clear();
        snapshot = stats_;
    }
    emit statsUpdated(snapshot);
    qInfo() << "[NetMonitor] statistics reset";
}

int NetMonitor::calculateJitterLocked() const
{
    if (rttHistory_.size() < 2) return 0;

    double average = 0.0;
    for (int rtt : rttHistory_) average += rtt;
    average /= rttHistory_.size();

    double variance = 0.0;
    for (int rtt : rttHistory_) {
        const double difference = rtt - average;
        variance += difference * difference;
    }
    return static_cast<int>(std::sqrt(variance / rttHistory_.size()));
}

int NetMonitor::calculateNetworkLevelLocked() const
{
    if (stats_.sessionState == rtmp::SessionState::Error ||
        stats_.sessionState == rtmp::SessionState::Stopped ||
        stats_.sessionState == rtmp::SessionState::Idle) {
        return 3;
    }
    if (stats_.videoQueueDelayMs >= 1000 || stats_.socketWriteBlockMs >= 500 ||
        stats_.rttMs >= 500) {
        return 3;
    }
    if (stats_.videoQueueDelayMs >= 400 || stats_.socketWriteBlockMs >= 150 ||
        stats_.rttMs >= 200 || stats_.jitterMs >= 80) {
        return 2;
    }
    if (stats_.videoQueueDelayMs >= 150 || stats_.socketWriteBlockMs >= 50 ||
        stats_.rttMs >= 120 || stats_.jitterMs >= 40) {
        return 1;
    }
    return 0;
}
