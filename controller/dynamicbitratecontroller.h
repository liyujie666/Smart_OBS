#ifndef DYNAMICBITRATECONTROLLER_H
#define DYNAMICBITRATECONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMutex>

#include "monitor/netmonitor.h"

class PacketQueue;
class VEncoder;
class AEncoder;

enum class BitrateAdjustState {
    Stable,
    Lowering,
    Raising
};

class DynamicBitrateController : public QObject
{
    Q_OBJECT
public:
    explicit DynamicBitrateController(
        PacketQueue* vPktQueue,
        PacketQueue* aPktQueue,
        VEncoder* vEncoder,
        AEncoder* aEncoder,
        NetMonitor* netMonitor,
        QObject* parent = nullptr);

    void init(int initVBitrate, int initABitrate = 128000, int minVBitrateRatio = 40);
    void start();
    void stop();

    int getCurrentVideoBitrate() const;
    BitrateAdjustState getAdjustState() const;

signals:
    void bitrateAdjusted(int oldBitrate, int newBitrate, BitrateAdjustState state);

private slots:
    void onStatsUpdated(const NetworkStats& stats);
    void onMonitorTimerTimeout();

private:
    bool isCongested(const NetworkStats& stats) const;
    bool isRecovered(const NetworkStats& stats) const;
    int congestionTarget(const NetworkStats& stats) const;
    bool applyVideoBitrate(int targetBitrate);

    PacketQueue* vPktQueue_ = nullptr;
    PacketQueue* aPktQueue_ = nullptr;
    VEncoder* vEncoder_ = nullptr;
    AEncoder* aEncoder_ = nullptr;
    NetMonitor* netMonitor_ = nullptr;

    int initVideoBitrate_ = 0;
    int initAudioBitrate_ = 0;
    int minVideoBitrate_ = 0;
    int currentVideoBitrate_ = 0;

    int congestedSamples_ = 0;
    int recoveredSamples_ = 0;
    int previousDroppedFrames_ = 0;
    int previousVideoQueueDelayMs_ = 0;
    qint64 previousBytesInflight_ = 0;
    qint64 latestStatsTimestamp_ = 0;
    qint64 lastAdjustTimestamp_ = 0;
    BitrateAdjustState adjustState_ = BitrateAdjustState::Stable;
    NetworkStats latestStats_;

    mutable QMutex mutex_;
    QTimer monitorTimer_;

    static constexpr int MonitorIntervalMs = 1000;
    static constexpr int LowerAdjustIntervalMs = 2500;
    static constexpr int RaiseAdjustIntervalMs = 10000;
    static constexpr int StatsStaleTimeoutMs = 3000;
    static constexpr int CongestedSamplesRequired = 2;
    static constexpr int RecoveredSamplesRequired = 8;
};

#endif // DYNAMICBITRATECONTROLLER_H
