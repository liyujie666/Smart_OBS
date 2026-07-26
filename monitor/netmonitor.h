#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <QObject>
#include <QMutex>
#include <QString>
#include <QList>

#include "rtmp/stats.h"

enum class NetworkStatus {
    Disconnected,
    Bad,
    NotBad,
    Normal,
    Good
};

struct NetworkStats {
    qint64 bytesSent = 0;
    qint64 bytesAcked = 0;
    qint64 bytesInflight = 0;
    int sendThroughputBps = 0;
    int encodeInputBps = 0;
    int rttMs = 0;
    int jitterMs = 0;
    int videoQueueDelayMs = 0;
    int audioQueueDelayMs = 0;
    int socketWriteBlockMs = 0;
    int droppedVideoFrames = 0;
    int requestedKeyframes = 0;
    int reconnectCount = 0;
    int networkLevel = 3;
    rtmp::SessionState sessionState = rtmp::SessionState::Idle;
    QString stateDetail;

    bool isStreaming() const { return sessionState == rtmp::SessionState::Streaming; }
};

Q_DECLARE_METATYPE(NetworkStats)
Q_DECLARE_METATYPE(rtmp::SessionState)

class NetMonitor : public QObject
{
    Q_OBJECT
public:
    static NetMonitor* instance();

    void updateRtmpStats(const rtmp::RtmpStats& stats);
    void updateSessionState(rtmp::SessionState state, const QString& detail = {});
    NetworkStats getCurrentStats() const;
    void reset();

signals:
    void statsUpdated(const NetworkStats& stats);
    void sessionStateChanged(rtmp::SessionState state, const QString& detail);

private:
    explicit NetMonitor(QObject* parent = nullptr);
    int calculateJitterLocked() const;
    int calculateNetworkLevelLocked() const;

    mutable QMutex mutex_;
    NetworkStats stats_;
    rtmp::SessionState previousState_ = rtmp::SessionState::Idle;
    bool hasStreamed_ = false;
    bool reconnectPending_ = false;
    qint64 lastStatsLogMs_ = 0;
    int lastLoggedNetworkLevel_ = -1;
    QList<int> rttHistory_;
};

#endif // NETMONITOR_H
