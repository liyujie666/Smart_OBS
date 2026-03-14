#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H

#include <QObject>
#include <QUdpSocket>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QElapsedTimer>
#include <QTimer>
#include <mutex>
#include <QJsonDocument>
#include <QJsonObject>
#include <vector>
enum class MessageType;
// 检测类型
enum class MonitorType{
    UDP_PING,               // UDP探测
    TCP_CONNECT,            // TCP端口连通性（延迟、是否可达）
    ZLMEDIAKIT // ZLMediakit：推流网络状态（丢包率、带宽）
};

enum class NetworkStatus{
    Disconnected,
    Bad,
    NotBad,
    Normal,
    Good
};

// 监测结果
struct NetworkMonitorResult{
    MonitorType type = MonitorType::ZLMEDIAKIT;
    bool isSuccess = false;
    int delayMs = -1;
    float lossRate = 0.0;
    int portStatus = 0;        // TCP端口状态(0=关闭，1=开放，-1=无数据)
    // ZLMediaKit 扩展字段
    int streamBitrateKbps = 0;   // 实时码率（Kbps）
    int lossFrame = 0;           // 丢帧数（客户端编码帧 - 服务端接收帧）
    qint64 totalFrames = 0;      // 总帧数
    qint64 totalDataBytes = 0;   // 总数据输出量（字节）
    QString streamName = "";
    QString errorMsg = "";
    bool isReconnect = false;
};

class VEncoder;

class NetworkMonitor : public QObject
{
    Q_OBJECT
public:
    // explicit NetworkMonitor(VEncoder* vEncoder = nullptr,QObject *parent = nullptr);
    explicit NetworkMonitor(QObject *parent = nullptr);
    ~NetworkMonitor();

    void setMonitorParams(MonitorType type,const QString& targetIp,quint16 tartgetPort,const QString &zlMediaStreamName = "",int tcpMaxReconnect = 10, int tcpReconnectIntervalMs = 3000);
    void start(int count = 10,int intervalMs = 1000);       // （count：探测包数，intervalMs：探测间隔/接口调用间隔）
    void stop();

    void setVEncoder(VEncoder* vEncoder);
    NetworkMonitorResult testTcpConnectivity(const QString& targetIp, quint16 targetPort, int timeoutMs = 2000);
    void statusBarMsg(const QString& text, MessageType type, int durationMs);
signals:
    // 实时监测结果（单次/单轮结果）
    void monitorRealTimeResult(const NetworkMonitorResult &result);
    // 最终统计结果（仅 UDP_PING/TCP_CONNECT 有，多次探测后的平均值）
    void monitorFinalResult(const NetworkMonitorResult &result);
    void tcpReconnected(const QString& ip, quint16 port);
    void serverDisconnected(MonitorType disconnectType, const QString& errorMsg);
    void serverClosed();
    void serverStatus(NetworkStatus status);


private slots:
    // UDP
    void sendUdpPingPacket();
    void onUdpReadyRead();
    // TCP
    void tcpConnectTest();          // 单次TCP探测
    void tcpReconnectTest();        // TCP重连尝试
    void onTcpConnected();
    void onTcpError(QAbstractSocket::SocketError error);
    // ZLMediakit
    void sendZlMediaApiRequest();
    void onZlMediaApiReply(QNetworkReply *reply);
    void onTimeout();
    void finishSingleMonitorRound();
    void onReconnectCountdownUpdate();


private:
    void init();
    void emitResultSignal(const NetworkMonitorResult &result, bool isFinal = false);
    void stopTcpReconnectTimer();
private:
    // 核心参数
    MonitorType currentMonitorType_;
    QString targetIp_;
    quint16 targetPort_;
    QString zlMediaStreamName_;       // ZLMediakit 推流名（如 "live/test"）
    QString zlMediaUrl_;              // ZLMediakit API 地址（拼接后）

    // 网络组件
    QTcpSocket* tcpSocket_;
    QUdpSocket* udpSocket_;
    QNetworkAccessManager* netManager_;

    // 定时器
    QTimer* probeTimer_;
    QTimer* timeoutTimer_;

    // 统计数据
    std::mutex stateMutex_;
    VEncoder* vEncoder_;
    int totalProbeCount_;
    int currentProbeCount_;
    int successProbeCount_;
    std::vector<int> delayList_;
    quint16 currentUdpSeq_;     // UDP 探测包序列号
    bool isMonitoring_ = false;
    int encodedFrames_ = 0;

    QTimer* tcpReconnectTimer_;    // TCP重连定时器
    int tcpMaxReconnect_;          // TCP最大重连次数
    int tcpReconnectIntervalMs_;   // TCP重连间隔（ms）
    int tcpReconnectCount_;        // 当前TCP重连次数
    bool isTcpReconnecting_;       // 是否正在TCP重连
    bool isUserStopTcp_;           // 是否用户主动停止TCP监测
    QElapsedTimer tcpConnectTimer_;// 计算TCP连接延迟
    QTimer *reconnectCountdownTimer_; // 倒计时定时器（1秒触发一次）
    int currentCountdownSec_;         // 当前倒计时秒数（从重连间隔转换而来）
    int tcpReconnectIntervalSec_;     // 重连间隔（秒，避免重复计算 ms→s）

    bool isServerDisconnected_ = false;
    int consecutiveFailCount_ = 0; // 连续失败次数（用于判定是否真断开）
    const int CONSECUTIVE_FAIL_THRESHOLD = 1; // 连续失败2次即判定为断开
};

#endif // NETWORKMONITOR_H
