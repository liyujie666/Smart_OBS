#include "statusbarmanager.h"
#include <QString>

StatusBarManager::StatusBarManager(QObject *parent)
    : QObject(parent),
    m_streamStatus(StreamStatus::Stopped),
    m_cpuUsage(0.0),
    m_frameRate(30.0),
    m_currentMsgType(MessageType::Info),
    m_msgTimer(new QTimer(this))
{
    // 初始化定时器
    m_msgTimer->setSingleShot(true);
    connect(m_msgTimer, &QTimer::timeout, this, &StatusBarManager::onMessageTimeout);

    // 初始化CPU监视器
    cpuMonitor_ = new CpuMonitor();
    cpuMonitor_->moveToThread(&cpuThread_);

    connect(&cpuThread_,&QThread::started,cpuMonitor_,&CpuMonitor::startMonitoring);
    connect(&cpuThread_,&QThread::finished,cpuMonitor_,&CpuMonitor::deleteLater);
    connect(cpuMonitor_,&CpuMonitor::processCpuUsageUpdated,this,&StatusBarManager::updateCpuUsage);
    cpuThread_.start();

    // 初始化状态文本
    updateStatusText();
}

StatusBarManager& StatusBarManager::getInstance()
{
    static StatusBarManager instance;
    return instance;
}

StatusBarManager::~StatusBarManager()
{
    if (cpuMonitor_) {
        cpuMonitor_->stopMonitoring();
    }
    cpuThread_.quit();
    cpuThread_.wait();
}

void StatusBarManager::showMessage(const QString& text, MessageType type, int durationMs)
{
    m_currentMessage = text;
    m_currentMsgType = type;

    // 启动消息定时器
    if (durationMs > 0) {
        m_msgTimer->start(durationMs);
    } else {
        m_msgTimer->stop();
    }

    // 通知UI显示临时消息
    emit statusUpdated(text, type, true);
}

void StatusBarManager::updateStreamStatus(StreamStatus status)
{
    if (m_streamStatus == status) return; // 状态未变化则不更新

    m_streamStatus = status;
    updateStatusText();


    emit statusInfoUpdated(m_recordText,m_streamText);

}

void StatusBarManager::updateCpuUsage(double usage)
{
    double clampedUsage = qBound(0.0, usage, 100.0); // 限制在0-100%之间
    if (qFuzzyCompare(m_cpuUsage, clampedUsage)) return; // 变化不大则不更新

    m_cpuUsage = clampedUsage;

    emit cpuInfoUpdated(m_cpuUsage);

}

void StatusBarManager::updateNetworkInfo(const NetworkStats& stats)
{
    emit streamNetworkInfoUpdated(stats);
    emit networkStatusUpdated(getNetworkStatus(stats));
}

void StatusBarManager::bindFPSCounter(FPSCounter *fpsCounter)
{
    connect(fpsCounter,&FPSCounter::fpsInfoUpdated,this,&StatusBarManager::updateFrameRate);
}

void StatusBarManager::bindNetMonitor(NetMonitor* netMonitor)
{
    if (!netMonitor || netMonitor_ == netMonitor) return;
    netMonitor_ = netMonitor;
    connect(netMonitor_, &NetMonitor::statsUpdated,
            this, &StatusBarManager::updateNetworkInfo,
            Qt::UniqueConnection);
}

void StatusBarManager::setIsPushing(bool isPush)
{
    if(isPush)
        emit showNetWorkLabel();
    else
        emit hideNetWorkLabel();

}

void StatusBarManager::setMaxFPS(int maxFPS)
{
    if(maxFPS > 0 && m_maxFPS != maxFPS)
    {
        m_maxFPS = maxFPS;
        if(m_streamStatus == StreamStatus::Stopped)
        {
            emit fpsInfoUpdated(maxFPS);
        }
    }
}


void StatusBarManager::updateFrameRate(double fps)
{
    // qDebug() << "updateFrameRate fps" << fps;
    double clampedFps = qMax(0.0, fps); // 确保帧率非负
    if (qFuzzyCompare(m_frameRate, clampedFps)) return; // 变化不大则不更新

    m_frameRate = clampedFps;

    emit fpsInfoUpdated(m_frameRate);

}

void StatusBarManager::onMessageTimeout()
{
    // 消息超时，清除消息并显示系统信息
    m_currentMessage.clear();
    emit statusUpdated("", MessageType::Info, false); // 通知UI清除临时消息
}

void StatusBarManager::updateStatusText()
{
    // 根据当前状态设置状态文本（仅显示状态，不包含时间）
    switch (m_streamStatus) {
    case StreamStatus::Recording:
        m_recordText = "录制中";
        m_streamText = "未推流";
        break;
    case StreamStatus::Streaming:
        m_streamText = "推流中";
        m_recordText = "未录制";
        break;
    case StreamStatus::RecPausedNOStr:
        m_recordText = "已暂停";
        m_streamText = "未推流";
        break;
    case StreamStatus::RecPausedStr:
        m_recordText = "已暂停";
        m_streamText = "推流中";
        break;
    case StreamStatus::Both:
        m_recordText = "录制中";
        m_streamText = "推流中";
        break;
    default:
        m_recordText = "未录制";
        m_streamText = "未推流";
    }
}

NetworkStatus StatusBarManager::getNetworkStatus(const NetworkStats& stats) const
{
    if (!stats.isStreaming()) return NetworkStatus::Disconnected;
    switch (stats.networkLevel) {
    case 0: return NetworkStatus::Good;
    case 1: return NetworkStatus::Normal;
    case 2: return NetworkStatus::NotBad;
    case 3: return NetworkStatus::Bad;
    default: return NetworkStatus::Disconnected;
    }
}

void StatusBarManager::release()
{
    m_msgTimer->stop();
    m_currentMessage.clear();
    emit statusUpdated("", MessageType::Info, false);

    if (cpuMonitor_ && cpuThread_.isRunning()) {
        cpuMonitor_->stopMonitoring();
        cpuThread_.quit();
        cpuThread_.wait(1000);
        cpuMonitor_ = nullptr;
    }

    m_streamStatus = StreamStatus::Stopped;
    m_cpuUsage = 0.0;
    m_frameRate = 0.0;
    m_maxFPS = 30;

    updateStatusText();
    emit statusInfoUpdated(m_recordText, m_streamText);
    emit cpuInfoUpdated(m_cpuUsage);
    emit fpsInfoUpdated(m_frameRate);
}
