#ifndef DYNAMICBITRATECONTROLLER_H
#define DYNAMICBITRATECONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include "monitor/networkmonitor.h" // 引入 NetworkMonitor 相关定义

class PacketQueue;
class VEncoder;
class AEncoder;

enum class BitrateAdjustState {
    Stable,     // 码率稳定
    Lowering,   // 正在降码率
    Raising     // 正在升码率
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
        NetworkMonitor* networkMonitor,
        QObject *parent = nullptr);
    ~DynamicBitrateController() = default;

    // 初始化码率配置（初始码率、最小码率比例）
    void init(
        int initVBitrate,
        int initABitrate = 128000,
        int minVBitrateRatio = 40);

    // 启动/停止码率控制
    void start();
    void stop();

    // 获取当前状态（供外部查询）
    int getCurrentVideoBitrate() const;
    BitrateAdjustState getAdjustState() const;

signals:
    // 码率调整通知（UI 可监听）
    void bitrateAdjusted(int oldBitrate, int newBitrate, BitrateAdjustState state);
public slots:
    // 处理 NetworkMonitor 的实时结果（所有监测类型）
    void onNetworkRealTimeResult(const NetworkMonitorResult& result);
    // 处理 NetworkMonitor 的最终结果（仅 UDP/TCP，作为辅助参考）
    void onNetworkFinalResult(const NetworkMonitorResult& result);
private slots:
    // 定时监测队列与网络状态（核心逻辑）
    void onMonitorTimerTimeout();


private:

    // 判断网络是否拥堵（队列堆积 + 网络异常）
    bool isNetworkCongested() const;
    // 判断网络是否恢复（队列空 + 网络正常）
    bool isNetworkRecovered() const;

    // 计算目标码率（平滑调整逻辑）
    int calculateTargetVideoBitrate();
    // 更新视频编码器码率
    bool updateVideoEncoderBitrate(int targetBitrate);
    // 按比例更新音频编码器码率
    bool updateAudioEncoderBitrate(int targetVideoBitrate);

private:
    PacketQueue* vPktQueue_ = nullptr;   // 视频包队列
    PacketQueue* aPktQueue_ = nullptr;   // 音频包队列
    VEncoder* vEncoder_ = nullptr;       // 视频编码器
    AEncoder* aEncoder_ = nullptr;       // 音频编码器
    NetworkMonitor* networkMonitor_ = nullptr; // 网络监测器

    int initVideoBitrate_ = 0;    // 初始视频码率（bps）
    int initAudioBitrate_ = 0;    // 初始音频码率（bps）
    int minVideoBitrate_ = 0;     // 最小视频码率（bps，初始码率的40%，保底500Kbps）
    int currentVideoBitrate_ = 0; // 当前视频码率（bps）
    int currentAudioBitrate_ = 0; // 当前音频码率（bps）

    const int VIDEO_QUEUE_THRESHOLD_ = 30;  // 视频队列超30帧 → 拥堵
    const int AUDIO_QUEUE_THRESHOLD_ = 50;  // 音频队列超50帧 → 拥堵
    const int QUEUE_EMPTY_THRESHOLD_ = 5;   // 队列≤5帧 → 空（恢复）
    const int MONITOR_INTERVAL_ = 1000;     // 1秒监测一次
    const float BITRATE_BANDWIDTH_RATIO_THRESHOLD_ = 0.7f;  // 带宽拥堵判断阈值（实际带宽低于理论带宽的 70% 判定为拥堵）


    const int ADJUST_INTERVAL_LOWER_ = 2000; // 降码率间隔（2秒）
    const int ADJUST_INTERVAL_RAISE_ = 3000; // 升码率间隔（3秒）
    qint64 lastAdjustTimestamp_ = 0;         // 上次调整时间（毫秒）
    BitrateAdjustState adjustState_ = BitrateAdjustState::Stable; // 当前调整状态

    mutable QMutex networkStateMutex_;  // 网络状态互斥锁
    float currentLossRate_ = 0.0f;      // 丢包率（%，默认0）
    int currentDelayMs_ = -1;           // 延迟（ms，-1=无数据）
    int currentPortStatus_ = -1;        // TCP端口状态（0=关闭，1=开放，-1=无数据）
    int currentStreamBitrate_ = -1;     // 推流带宽（Kbps，ZLMEDIAKIT专属）

    QTimer monitorTimer_;
};

#endif // DYNAMICBITRATECONTROLLER_H
