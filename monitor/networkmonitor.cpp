#include "networkmonitor.h"
#include "statusbar/statusbarmanager.h"
#include <QDebug>
#include <QHostAddress>
#include <QRegularExpression>
#include <QJsonArray>
#include <QNetworkProxy>
#include "encoder/vencoder.h"
NetworkMonitor::NetworkMonitor(QObject *parent)
    : QObject{parent}
{
    tcpSocket_ = new QTcpSocket(this);
    udpSocket_ = new QUdpSocket(this);
    netManager_ = new QNetworkAccessManager(this);

    // 定时器
    probeTimer_ = new QTimer(this);
    probeTimer_->setSingleShot(false);
    timeoutTimer_ = new QTimer(this);
    timeoutTimer_->setSingleShot(true);
    timeoutTimer_->setInterval(1500);
    tcpReconnectTimer_ = new QTimer(this);
    tcpReconnectTimer_->setSingleShot(true);
    reconnectCountdownTimer_ = new QTimer(this);
    reconnectCountdownTimer_->setInterval(1000);
    reconnectCountdownTimer_->setSingleShot(false);

    // 连接 TCP 信号
    connect(tcpSocket_, &QTcpSocket::connected, this, &NetworkMonitor::onTcpConnected);
    connect(tcpSocket_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),this, &NetworkMonitor::onTcpError);
    connect(netManager_, &QNetworkAccessManager::finished, this, &NetworkMonitor::onZlMediaApiReply);



    // 连接定时器信号
    connect(probeTimer_, &QTimer::timeout, this, [this]() {
        switch (currentMonitorType_) {
        case MonitorType::UDP_PING:
            sendUdpPingPacket();
            break;
        case MonitorType::TCP_CONNECT:
            if (!isTcpReconnecting_) tcpConnectTest();
            break;
        case MonitorType::ZLMEDIAKIT:
            sendZlMediaApiRequest();
            break;
        }
    });

    connect(timeoutTimer_, &QTimer::timeout, this, &NetworkMonitor::onTimeout);
    connect(tcpReconnectTimer_, &QTimer::timeout, this, &NetworkMonitor::tcpReconnectTest);
    connect(reconnectCountdownTimer_, &QTimer::timeout, this, &NetworkMonitor::onReconnectCountdownUpdate);
}

NetworkMonitor::~NetworkMonitor()
{
    stop();
}

void NetworkMonitor::setMonitorParams(MonitorType type, const QString &targetIp, quint16 tartgetPort, const QString &zlMediaStreamName,int tcpMaxReconnect, int tcpReconnectIntervalMs)
{
    currentMonitorType_ = type;
    targetIp_ = targetIp;
    targetPort_ = tartgetPort;
    zlMediaStreamName_ = zlMediaStreamName;

    if(type == MonitorType::ZLMEDIAKIT){
        QString schema = "rtmp";                  // 推流协议（RTMP）
        QString vhost = "__defaultVhost__";       // 默认虚拟主机
        QString app = "live";                     // 应用名（从推流地址提取）
        QString secret = "83rIDpNVawCmxHzsVnMG6kk7pDaBgbg6"; // 鉴权密钥

        // 2. 拼接完整 URL（包含所有必填参数）
        zlMediaUrl_ = QString("http://%1:%2/index/api/getMediaInfo")
                          .arg(targetIp_)
                          .arg(targetPort_)
                          .append("?schema=").append(schema)
                          .append("&vhost=").append(vhost)
                          .append("&app=").append(app)
                          .append("&stream=").append(zlMediaStreamName_)
                          .append("&secret=").append(secret);

        qDebug() << "ZLMediakit API 地址（完整参数）：" << zlMediaUrl_;
    }else if (type == MonitorType::TCP_CONNECT) {
        tcpMaxReconnect_ = tcpMaxReconnect;
        tcpReconnectIntervalMs_ = tcpReconnectIntervalMs;
        tcpReconnectIntervalSec_ = tcpReconnectIntervalMs_ / 1000;
        isUserStopTcp_ = false;
        isTcpReconnecting_ = false;
        tcpReconnectCount_ = 0;
    }

}

void NetworkMonitor::start(int count, int intervalMs)
{
    if(isMonitoring_) return;

    if(targetIp_.isEmpty() || targetPort_ == 0){
        NetworkMonitorResult result;
        result.type = currentMonitorType_;
        result.isSuccess = false;
        result.errorMsg = "未设置目标 IP 或端口";
        emitResultSignal(result);
        return;
    }

    init();
    totalProbeCount_ = count;
    probeTimer_->setInterval(intervalMs);
    isMonitoring_ = true;

    switch (currentMonitorType_) {
    case MonitorType::TCP_CONNECT:tcpConnectTest();break;
    case MonitorType::UDP_PING:sendUdpPingPacket();break;
    case MonitorType::ZLMEDIAKIT:sendZlMediaApiRequest(); qDebug() << "初始调用 sendZlMediaApiRequest（start 中）";break;
    default:break;
    }

    probeTimer_->start();
}


void NetworkMonitor::stop()
{
    if(!isMonitoring_) return;
    probeTimer_->stop();
    timeoutTimer_->stop();
    stopTcpReconnectTimer(); // 停止TCP重连定时器

    if (reconnectCountdownTimer_->isActive()) {
        reconnectCountdownTimer_->stop();
    }
    // 原有停止逻辑（不变）
    udpSocket_->abort();
    tcpSocket_->abort();
    netManager_->clearAccessCache();

    // TCP专属：标记用户主动停止
    if (currentMonitorType_ == MonitorType::TCP_CONNECT) {
        isUserStopTcp_ = true;
        isTcpReconnecting_ = false;
        tcpReconnectCount_ = 0;
    }

    if (isMonitoring_) {
        QTimer::singleShot(0, this, &NetworkMonitor::finishSingleMonitorRound);
        isMonitoring_ = false;
    }
}

void NetworkMonitor::setVEncoder(VEncoder *vEncoder)
{
    if(!vEncoder_) return;
    vEncoder_ = vEncoder;
}


void NetworkMonitor::stopTcpReconnectTimer()
{
    if (tcpReconnectTimer_->isActive()) {
        tcpReconnectTimer_->stop();
    }
}




void NetworkMonitor::sendUdpPingPacket()
{
    std::lock_guard<std::mutex> locker(stateMutex_);
    if(currentProbeCount_ >= totalProbeCount_){
        probeTimer_->stop();
        QTimer::singleShot(1000,this,&NetworkMonitor::finishSingleMonitorRound);
        return;
    }

    // 构建UDP探测包
    currentUdpSeq_++;
    QString dataStr = QString("NetProbe:%1|%2").arg(currentUdpSeq_).arg(QDateTime::currentMSecsSinceEpoch());
    QByteArray data = dataStr.toUtf8();

    // 发送Udp包
    qint64 byteSent = udpSocket_->writeDatagram(data,QHostAddress(targetIp_),targetPort_);
    if(byteSent != data.size()){
        NetworkMonitorResult result;
        result.type = MonitorType::UDP_PING;
        result.isSuccess = false;
        result.delayMs = -1;
        result.lossRate = -1.0;
        result.errorMsg = QString("UDP 包发送失败（序列号：%1）").arg(currentUdpSeq_);
        emitResultSignal(result);
        return;
    }

    currentProbeCount_++;
    timeoutTimer_->start(); // 启动超时计时
    qDebug() << "UDP 探测包发送（序列号：" << currentUdpSeq_ << "）";
}

void NetworkMonitor::onUdpReadyRead()
{
    while(udpSocket_->hasPendingDatagrams()){
        QByteArray datagram;
        QHostAddress senderAddr;
        quint16 senderPort;
        datagram.resize(udpSocket_->pendingDatagramSize());
        udpSocket_->readDatagram(datagram.data(),datagram.size(),&senderAddr,&senderPort);

        if (senderAddr.toString() != targetIp_ || senderPort != targetPort_) {
            continue;
        }

        // 解析响应包
        QString dataStr = QString::fromUtf8(datagram);
        QRegularExpression reg("NetProbe:(\\d+)\\|(\\d+)");
        QRegularExpressionMatch match = reg.match(dataStr);
        if (!match.hasMatch()) {
            continue;
        }

        // 计算延迟（当前时间 - 发送时间）
        timeoutTimer_->stop();
        quint16 respSeq = match.captured(1).toInt();
        qint64 sendTime = match.captured(2).toLongLong();
        int delayMs = QDateTime::currentMSecsSinceEpoch() - sendTime;

        std::lock_guard<std::mutex> locker(stateMutex_);
        successProbeCount_++;
        delayList_.push_back(delayMs);

        // 发送实时结果
        NetworkMonitorResult result;
        result.type = MonitorType::UDP_PING;
        result.isSuccess = true;
        result.delayMs = delayMs;
        result.lossRate = (currentProbeCount_ == 0) ? 0.0 : 100.0 - (100.0 * successProbeCount_ / totalProbeCount_);

        emitResultSignal(result);
        qDebug() << "UDP 响应接收（序列号：" << respSeq << "），延迟：" << delayMs << "ms";
    }
}

NetworkMonitorResult NetworkMonitor::testTcpConnectivity(const QString& targetIp, quint16 targetPort, int timeoutMs)
{
    NetworkMonitorResult result;
    result.type = MonitorType::TCP_CONNECT;
    QTcpSocket tempSocket;

    // 发起临时TCP连接
    tempSocket.connectToHost(targetIp, targetPort);
    QElapsedTimer timer;
    timer.start();

    // 等待连接结果
    if (tempSocket.waitForConnected(timeoutMs)) {
        result.isSuccess = true;
        result.delayMs = timer.elapsed();
        result.portStatus = 1; // 端口开放
        result.errorMsg = "连接成功";
        tempSocket.disconnectFromHost();
    } else {
        result.isSuccess = false;
        result.delayMs = -1;
        result.portStatus = 0; // 端口关闭/不可达
        result.errorMsg = tempSocket.errorString();
    }

    return result;
}

void NetworkMonitor::statusBarMsg(const QString &text, MessageType type, int durationMs)
{
    StatusBarManager::getInstance().showMessage(text,type,durationMs);
}


void NetworkMonitor::tcpConnectTest()
{
    std::lock_guard<std::mutex> locker(stateMutex_);
    if (totalProbeCount_ > 0 && currentProbeCount_ >= totalProbeCount_) {
        probeTimer_->stop();
        QTimer::singleShot(1000, this, &NetworkMonitor::finishSingleMonitorRound);
        return;
    }

    tcpSocket_->abort();
    tcpConnectTimer_.restart();
    tcpSocket_->connectToHost(targetIp_, targetPort_);

    // 计数（仅固定次数监测用）
    if (totalProbeCount_ > 0) currentProbeCount_++;
    timeoutTimer_->start(); // 启动单次探测超时
    qDebug() << "[TCP监测] 发起探测（次数：" << (totalProbeCount_>0 ? QString::number(currentProbeCount_) : "无限") << "）";
}

void NetworkMonitor::tcpReconnectTest()
{
    std::lock_guard<std::mutex> locker(stateMutex_);
    if (reconnectCountdownTimer_->isActive()) {
        reconnectCountdownTimer_->stop();
    }

    // 检查是否达最大重连次数
    if (tcpReconnectCount_ >= tcpMaxReconnect_) {
        isTcpReconnecting_ = false;
        tcpReconnectCount_ = 0;
        NetworkMonitorResult result;
        result.type = MonitorType::TCP_CONNECT;
        result.isSuccess = false;
        result.isReconnect = true;
        result.errorMsg = QString("TCP重连失败（已达最大次数：%1次）").arg(tcpMaxReconnect_);
        emitResultSignal(result);
        qDebug() << result.errorMsg;
        return;
    }

    statusBarMsg("服务器重连中...", MessageType::Info, 4000);

    // 发起重连
    tcpSocket_->abort();
    tcpConnectTimer_.restart();
    tcpSocket_->connectToHost(targetIp_, targetPort_);
    tcpReconnectCount_++;
    timeoutTimer_->start(); // 重连超时计时
    isTcpReconnecting_ = true;
    qDebug() << "[TCP重连] 尝试第" << tcpReconnectCount_ << "次（间隔：" << tcpReconnectIntervalMs_ << "ms）";

    // 构造重连中结果
    // NetworkMonitorResult result;
    // result.type = MonitorType::TCP_CONNECT;
    // result.isSuccess = false;
    // result.isReconnect = true;
    // result.errorMsg = QString("TCP重连中（第%1次）").arg(tcpReconnectCount_);
    // emitResultSignal(result);

}
void NetworkMonitor::onTcpConnected()
{
    timeoutTimer_->stop();
    int delayMs = tcpConnectTimer_.elapsed(); // 从属性获取延迟（需在连接前设置）
    NetworkMonitorResult result;
    result.type = MonitorType::TCP_CONNECT;
    result.isSuccess = true;
    result.delayMs = delayMs;
    result.portStatus = 1;
    result.lossRate = -1.0;

    std::lock_guard<std::mutex> locker(stateMutex_);
    if (isTcpReconnecting_) {
        result.isReconnect = true;
        result.errorMsg = QString("TCP重连成功（延迟：%1ms）").arg(delayMs);
        statusBarMsg("服务器重连成功！", MessageType::Success, 5000);
        // 重连成功：重置重连状态，恢复持续监测
        isTcpReconnecting_ = false;
        tcpReconnectCount_ = 0;
        stopTcpReconnectTimer();
        isServerDisconnected_ = false;
        consecutiveFailCount_ = 0;
        emit tcpReconnected(targetIp_, targetPort_);
        qDebug() << "[TCP重连] 成功（延迟：" << delayMs << "ms）";
    } else {
        result.isReconnect = false;
        result.errorMsg = QString("TCP监测成功（延迟：%1ms）").arg(delayMs);
        successProbeCount_++;
        delayList_.push_back(delayMs);
        qDebug() << "[TCP监测] 成功（延迟：" << delayMs << "ms）";
    }

    emitResultSignal(result);
    tcpSocket_->disconnectFromHost(); // 探测/重连后断开（仅需验证连通性，无需保持连接）
}

void NetworkMonitor::onTcpError(QAbstractSocket::SocketError error)
{
    timeoutTimer_->stop();
    QString errorMsg = tcpSocket_->errorString();
    NetworkMonitorResult result;
    result.type = MonitorType::TCP_CONNECT;
    result.isSuccess = false;
    result.delayMs = -1;
    result.portStatus = 0;
    result.lossRate = -1.0;

    std::lock_guard<std::mutex> locker(stateMutex_);
    consecutiveFailCount_++;
    qDebug() << "[TCP监测] 连续失败次数：" << consecutiveFailCount_;

    // 关键修复1：服务器断开判定分支——优先处理，强制重置重连状态
    if (consecutiveFailCount_ >= CONSECUTIVE_FAIL_THRESHOLD && !isUserStopTcp_ && !isServerDisconnected_) {
        isServerDisconnected_ = true;
        // 强制重置重连状态，避免被之前的重连逻辑干扰
        isTcpReconnecting_ = true;
        tcpReconnectCount_ = 0; // 初始化重连次数
        currentCountdownSec_ = tcpReconnectIntervalSec_;

        // 发送首次断连文本（确保这是当前最优先的文本）
        QString firstDisconnectText = QString("服务器已断开，正在尝试第1次重连[%1s]...")
                                          .arg(currentCountdownSec_);
        statusBarMsg(firstDisconnectText, MessageType::Info, 1000);
        reconnectCountdownTimer_->start();
        // 启动首次重连定时器
        tcpReconnectTimer_->start(tcpReconnectIntervalMs_);

        emit serverDisconnected(MonitorType::TCP_CONNECT, errorMsg);
        qDebug() << "[服务器断开] 发送首次断连文本";
        // 直接返回，避免进入后续重连失败分支
        emitResultSignal(result);
        return;
    }

    // 区分“正常监测失败”和“重连失败”
    if (isTcpReconnecting_) {
        result.isReconnect = true;
        result.errorMsg = QString("TCP重连失败（第%1次）：%2").arg(tcpReconnectCount_).arg(errorMsg);
        // 重连失败：启动下一次重连（未达最大次数）
        if (tcpReconnectCount_ < tcpMaxReconnect_) {
            currentCountdownSec_ = tcpReconnectIntervalSec_;
            QString failText = QString("重连失败，正在尝试第%1次重连[%2s]...")
                                   .arg(tcpReconnectCount_ + 1)
                                   .arg(currentCountdownSec_);
            statusBarMsg(failText, MessageType::Info, 1000);
            reconnectCountdownTimer_->start();
            tcpReconnectTimer_->start(tcpReconnectIntervalMs_);
            qDebug() << "[重连失败] 发送重连失败文本";
        } else {
            // 无剩余次数，显示最终失败
            QString finalFailText = QString("重连失败，已达最大重连次数（%1次），已关闭推流").arg(tcpMaxReconnect_);
            statusBarMsg(finalFailText, MessageType::Error, 5000);
            isTcpReconnecting_ = false;
            emit serverClosed();
        }
    } else {
        result.isReconnect = false;
        result.errorMsg = QString("TCP监测失败：%1").arg(errorMsg);
        if (!isUserStopTcp_ && isMonitoring_ && consecutiveFailCount_ < CONSECUTIVE_FAIL_THRESHOLD) {
            isTcpReconnecting_ = true;
            tcpReconnectTimer_->start(tcpReconnectIntervalMs_);
        }
    }

    emitResultSignal(result);
    qDebug() << result.errorMsg;
}


void NetworkMonitor::sendZlMediaApiRequest()
{
    if (zlMediaUrl_.isEmpty()) {
        NetworkMonitorResult result;
        result.type = MonitorType::ZLMEDIAKIT;
        result.isSuccess = false;
        result.errorMsg = "ZLMediakit API 地址未设置";
        emitResultSignal(result);
        return;
    }
    QNetworkRequest request{QUrl(zlMediaUrl_)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    netManager_->get(request);
}

void NetworkMonitor::onZlMediaApiReply(QNetworkReply *reply)
{
    NetworkMonitorResult result;
    result.type = MonitorType::ZLMEDIAKIT;
    result.streamName = zlMediaStreamName_;
    result.isSuccess = false; // 初始设为失败，解析成功后改为true
    qint64 currentMonitorTime = QDateTime::currentMSecsSinceEpoch();

    // 1. 先处理网络错误
    if (reply->error() != QNetworkReply::NoError) {
        result.errorMsg = QString("API 请求失败：%1").arg(reply->errorString());
        emitResultSignal(result);
        reply->deleteLater();
        return;
    }

    // 2. 解析 JSON 响应
    QByteArray responseData = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isObject()) {
        result.errorMsg = "API 响应不是 JSON 格式";
        emitResultSignal(result);
        reply->deleteLater();
        return;
    }

    QJsonObject jsonObj = jsonDoc.object();
    int code = jsonObj["code"].toInt(-1);
    if (code != 0) {
        result.errorMsg = QString("API 业务失败：code=%1，msg=%2")
                              .arg(code)
                              .arg(jsonObj["msg"].toString("未知错误"));
        emitResultSignal(result);
        reply->deleteLater();
        return;
    }

    std::lock_guard<std::mutex> locker(stateMutex_);
    consecutiveFailCount_ = 0;
    isServerDisconnected_ = false;

    // 验证推流数据是否存在
    if (!jsonObj.contains("app") || !jsonObj.contains("stream")) {
        result.errorMsg = "API 响应缺少关键字段（app/stream），推流数据无效";
        emitResultSignal(result);
        reply->deleteLater();
        return;
    }

    // 提取总数据输出量
    result.totalDataBytes = jsonObj["totalBytes"].toVariant().toLongLong();

    // 提取视频轨道数据
    int currentFrames = 0;       // 当前累计帧数
    double currentFps = 30.0;    // 当前帧率
    bool hasVideoTrack = false;  // 是否找到视频轨道

    if (jsonObj.contains("tracks") && jsonObj["tracks"].isArray()) {
        QJsonArray tracks = jsonObj["tracks"].toArray();
        for (const auto& trackVal : tracks) {
            if (trackVal.isObject()) {
                QJsonObject track = trackVal.toObject();
                // 找到视频轨道（codec_type=0 表示视频）
                if (track["codec_type"].toInt(-1) == 0) {
                    currentFrames = track["frames"].toInt(0);
                    result.totalFrames = currentFrames;
                    currentFps = track["fps"].toDouble(30.0);
                    hasVideoTrack = true;
                    break;
                }
            }
        }
    }

    // 计算延迟）
    result.delayMs = -1; // 默认无有效数据
    // 计算丢弃的帧
    if(vEncoder_){
        result.lossFrame = vEncoder_->getEncodedFrameCount() - currentFrames;
        if (result.lossFrame < 0) result.lossFrame = 0;
    }



    // 提取丢包率和带宽
    result.lossRate = 0.0f;
    if (jsonObj.contains("tracks") && jsonObj["tracks"].isArray()) {
        QJsonArray tracks = jsonObj["tracks"].toArray();
        for (const auto& trackVal : tracks) {
            if (trackVal.isObject()) {
                QJsonObject track = trackVal.toObject();
                if (track["codec_type"].toInt(-1) == 0) { // 视频轨道
                    double loss = track["loss"].toDouble(-1.0);
                    result.lossRate = (loss < 0) ? 0.0f : static_cast<float>(loss);
                    break;
                }
            }
        }
    }

    if (jsonObj.contains("bytesSpeed")) {
        qint64 bytesPerSec = jsonObj["bytesSpeed"].toVariant().toLongLong();
        result.streamBitrateKbps = static_cast<int>((bytesPerSec * 8) / 1000);
    } else {
        result.streamBitrateKbps = -1;
    }

    // 7. 解析成功，发送结果
    result.isSuccess = true;
    emitResultSignal(result);
    qDebug() << "ZLMediakit API 解析成功："
             << "延迟=" << (result.delayMs == -1 ? "无有效数据" : QString::number(result.delayMs) + "ms") << "，"
             << "丢包率=" << result.lossRate << "%，"
             << "带宽=" << result.streamBitrateKbps << "Kbps，"
             << "累计帧数=" << currentFrames << "，"
             << "帧率=" << currentFps;

    reply->deleteLater();
}

void NetworkMonitor::onTimeout()
{
    NetworkMonitorResult result;
    result.type = currentMonitorType_;
    result.isSuccess = false;
    result.delayMs = -1;
    result.lossRate = -1.0;
    result.portStatus = -1;

    std::lock_guard<std::mutex> locker(stateMutex_);
    if (currentMonitorType_ == MonitorType::TCP_CONNECT) {
        // 新增：连续失败计数
        consecutiveFailCount_++;
        qDebug() << "[TCP监测] 连续超时次数：" << consecutiveFailCount_;

        // 判定服务器断开
        if (consecutiveFailCount_ >= CONSECUTIVE_FAIL_THRESHOLD && !isUserStopTcp_ && !isServerDisconnected_) {
            isServerDisconnected_ = true;
            emit serverDisconnected(MonitorType::TCP_CONNECT, result.errorMsg);
            qDebug() << "[服务器断开] TCP连接连续超时，判定为服务器断开";
        }
    }


    switch (currentMonitorType_) {
    case MonitorType::UDP_PING:
        result.errorMsg = QString("UDP 探测超时（序列号：%1）").arg(currentUdpSeq_);
        break;
    case MonitorType::TCP_CONNECT:
        result.errorMsg = isTcpReconnecting_
                              ? QString("TCP重连超时（第%1次）").arg(tcpReconnectCount_)
                              : QString("TCP监测超时（次数：%1）").arg(currentProbeCount_);
        result.portStatus = 0;
        // TCP超时触发重连
        if (!isTcpReconnecting_ && !isUserStopTcp_ && isMonitoring_) {
            isTcpReconnecting_ = true;
            tcpReconnectTimer_->start(tcpReconnectIntervalMs_);
        } else if (isTcpReconnecting_ && tcpReconnectCount_ < tcpMaxReconnect_) {
            tcpReconnectTimer_->start(tcpReconnectIntervalMs_);
        }
        result.isReconnect = isTcpReconnecting_;
        break;
    case MonitorType::ZLMEDIAKIT:
        result.errorMsg = "ZLMediakit API 请求超时";
        break;
    }

    emitResultSignal(result);
    qDebug() << result.errorMsg;
}

void NetworkMonitor::finishSingleMonitorRound()
{
    std::lock_guard<std::mutex> locker(stateMutex_);

    NetworkMonitorResult finalResult;
    if(currentMonitorType_ != MonitorType::ZLMEDIAKIT){
        if (currentProbeCount_ == 0) return;
        finalResult.type = currentMonitorType_;
        finalResult.isSuccess = (successProbeCount_ > 0);
        finalResult.lossRate = 100.0 - (100.0 * successProbeCount_ / currentProbeCount_);

        // 计算平均延迟
        if (!delayList_.empty()) {
            int delaySum = 0;
            for (int delay : delayList_) delaySum += delay;
            finalResult.delayMs = delaySum / delayList_.size();
        } else {
            finalResult.delayMs = -1;
        }

        // TCP 专属：端口状态（只要有一次成功，就认为端口开放）
        if (currentMonitorType_ == MonitorType::TCP_CONNECT) {
            finalResult.portStatus = (successProbeCount_ > 0) ? 1 : 0;
        }
        qDebug() << "监测轮次结束（类型：" << static_cast<int>(currentMonitorType_) << "），平均延迟：" << finalResult.delayMs << "ms，丢包率：" << finalResult.lossRate << "%";
    }

    emitResultSignal(finalResult, true);

}

void NetworkMonitor::onReconnectCountdownUpdate()
{
    std::lock_guard<std::mutex> locker(stateMutex_);
    currentCountdownSec_--;

    // 倒计时结束（该开始重连），停止定时器（重连逻辑由tcpReconnectTimer_触发）
    if (currentCountdownSec_ <= 0) {
        reconnectCountdownTimer_->stop();
        return;
    }

    QString baseText = tcpReconnectCount_ == 0 ? "服务器已断开，正在尝试第%1次重连[%2s]..." : "重连失败，正在尝试第%1次重连[%2s]...";

    QString finalText = QString(baseText)
                            .arg(tcpReconnectCount_ + 1)  // 重连次数：下次是第N+1次
                            .arg(currentCountdownSec_);   // 剩余倒计时秒数

    statusBarMsg (finalText, MessageType::Info, 1000);
}


void NetworkMonitor::init()
{
    std::lock_guard<std::mutex> locker(stateMutex_);
    currentProbeCount_ = 0;
    successProbeCount_ = 0;
    delayList_.clear();
    currentUdpSeq_ = 0;
    encodedFrames_ = 0;

    isServerDisconnected_ = false;
    consecutiveFailCount_ = 0;

}

void NetworkMonitor::emitResultSignal(const NetworkMonitorResult &result, bool isFinal)
{
    if (isFinal) {
        emit monitorFinalResult(result);
        qDebug() << "emit monitorFinalResult";
    } else {
        emit monitorRealTimeResult(result);
    }
}
