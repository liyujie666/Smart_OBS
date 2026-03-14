#include "statisticsdialog.h"
#include "ui_statisticsdialog.h"
#include <QTableWidgetItem>
#include "encoder/vencoder.h"
#include "statusbar/fpscounter.h"
StatisticsDialog* StatisticsDialog::s_instance = nullptr;

StatisticsDialog::StatisticsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::StatisticsDialog)
    , refreshTimer_(this)
{
    ui->setupUi(this);
    setWindowTitle("统计");
    // 初始化默认数据
    globalConfig_ = {0.0,0.0, 30,30,0,0.0,0,0.0,0,0,0,0};
    recordConfig_ = {StatisticType::Record, false, 0, 0, 0.0,0,0};
    streamConfig_ = {StatisticType::Stream, false, 0, 0, 0.0,0,0};

    initTableWidget();
    // 启动1秒定时器，定时刷新UI
    refreshTimer_.setInterval(1000);
    connect(&refreshTimer_, &QTimer::timeout, this, &StatisticsDialog::onRefreshTimerTimeout);
    refreshTimer_.start();
}

StatisticsDialog::~StatisticsDialog() {
    clear();
    delete ui;
}

void StatisticsDialog::initTableWidget() {
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableWidget->verticalHeader()->setDefaultSectionSize(40);
    ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    ui->tableWidget->setColumnWidth(0, 80);
    ui->tableWidget->setColumnWidth(1, 80);
    ui->tableWidget->setColumnWidth(2, 130);
    ui->tableWidget->setColumnWidth(3, 120);
    ui->tableWidget->setColumnWidth(4, 110);

    // 初始化表格行（录制、推流各一行）
    ui->tableWidget->setRowCount(2);
    QTableWidgetItem* recordItem = new QTableWidgetItem("录制");
    recordItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(0, 0, recordItem);
    QTableWidgetItem* streamItem = new QTableWidgetItem("推流");
    streamItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(1, 0, streamItem);

    updateSceneConfig(StatisticType::Record, recordConfig_);
    updateSceneConfig(StatisticType::Stream, streamConfig_);
}

void StatisticsDialog::setSystemMonitors(SystemMonitor* systemMonitor) {
    if(!systemMonitor) return;
    systemMonitor_ = systemMonitor;
}

void StatisticsDialog::setNetWorkMonitors(NetworkMonitor* networkMonitor) {
    if(!networkMonitor) return;
    networkMonitor_ = networkMonitor;

    // 连接NetworkMonitor的实时结果信号，更新推流数据
    if (networkMonitor_) {
        connect(networkMonitor_, &NetworkMonitor::monitorRealTimeResult,this, &StatisticsDialog::onStreamStatusUpdated);
        connect(networkMonitor_, &NetworkMonitor::monitorFinalResult,this, &StatisticsDialog::onStreamStatusUpdated);
    }
}

void StatisticsDialog::setEncodeConfig(VEncoder *vEncoder,FPSCounter* fpsCounter)
{
    if(!vEncoder || !fpsCounter) return;
    vEncoder_ = vEncoder;
    connect(fpsCounter,&FPSCounter::fpsInfoUpdated,this,&StatisticsDialog::onUpdateFps);
}
void StatisticsDialog::onRefreshTimerTimeout() {
    // 更新全局系统数据
    if (systemMonitor_) {
        SystemMetrics metrics = systemMonitor_->getCurrentMetrics();
        // CPU
        globalConfig_.cpuUsage = metrics.cpu_usage;
        globalConfig_.processCpuUsage = metrics.process_cpu_usage;

        // 磁盘占用
        globalConfig_.diskSpace = metrics.free_disk;
        globalConfig_.diakUsage = metrics.disk_usage;
        // 内存占用
        globalConfig_.processMemory = metrics.process_memory / (1024.0 * 1024.0);
        globalConfig_.memoryUsage = metrics.memory_usage;
        // 渲染延迟的帧
        globalConfig_.delayRenderedFrames = 0;
        globalConfig_.totalRenderedFrames = 0;
        // 编码延迟的帧
        if(vEncoder_){
            globalConfig_.expectFps = vEncoder_->getConfig().framerate;
            globalConfig_.delayEncodedFrames = vEncoder_->getDelayedFrameCount();
            globalConfig_.totalEncodedFrames = vEncoder_->getEncodedFrameCount();
        }


        updateGlobalConfig(globalConfig_);
    }




}

void StatisticsDialog::onStreamStatusUpdated(const NetworkMonitorResult& result) {
    if (result.type != MonitorType::ZLMEDIAKIT) return;

    // 解析ZLMEDIAKIT的监测结果，填充推流SceneConfig
    SceneConfig config;
    if(result.isSuccess){
        config.type = StatisticType::Stream;
        config.isActive = result.isSuccess; // 推流是否成功（在线）
        config.lossFrame = result.lossFrame;
        config.totalFrames = result.totalFrames;
        config.lossRate = result.lossRate;
        // 总输出数据量：totalBytes 转换为 MiB（1 MiB = 1024*1024 字节）
        config.storageSize = result.totalDataBytes;
        config.bitRate = result.streamBitrateKbps; // 实时码率（kb/s）
    }else
    {
        config.type = StatisticType::Stream;
        config.isActive = result.isSuccess; // 推流是否成功（在线）
        config.lossFrame = 0;
        config.totalFrames = 0;
        config.lossRate = 0.0;
        config.storageSize = 0;
        config.bitRate = 0;
    }

    updateSceneConfig(StatisticType::Stream, config);
}

void StatisticsDialog::onUpdateFps(int fps)
{
    globalConfig_.fps = fps;
}

// 更新全局配置UI
void StatisticsDialog::updateGlobalConfig(const GlobalConfig& config) {
    globalConfig_ = config;
    ui->cpuLabel->setText(QString("%1% / %2%").arg(config.processCpuUsage, 0, 'f', 1).arg(config.cpuUsage, 0, 'f', 1));
    ui->fpsLabel->setText(QString("%1 / %2").arg(config.fps).arg(config.expectFps));
    ui->diskSpaceLabel->setText(QString("%1(%2%)").arg(formatBytes(config.diskSpace)).arg(config.diakUsage,0,'f',1));
    ui->memoryLabel->setText(QString("%1(%2%)").arg(config.processMemory).arg(config.memoryUsage, 0, 'f', 1));
    ui->delayEncodedFrameLabel->setText(QString("%1 / %2").arg(config.delayEncodedFrames).arg(config.totalEncodedFrames));
    ui->delayRenderedFrameLabel->setText(QString("%1 / %2").arg(config.delayRenderedFrames).arg(config.totalRenderedFrames));
}

// 更新场景（录制/推流）UI
void StatisticsDialog::updateSceneConfig(StatisticType type, const SceneConfig& config)
{
    int row = (type == StatisticType::Record) ? 0 : 1;

    // 状态列
    auto *statusItem = new QTableWidgetItem(config.isActive ? "活跃" : "未激活");
    statusItem->setTextAlignment(Qt::AlignCenter);
    statusItem->setForeground(config.isActive ? QBrush(Qt::green) : QBrush(Qt::red));
    ui->tableWidget->setItem(row, 1, statusItem);

    // 丢帧数列
    auto *lossItem = new QTableWidgetItem(
        QString("%1 / %2 (%3%)")
            .arg(config.lossFrame)
            .arg(config.totalFrames)
            .arg(config.lossRate));
    lossItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(row, 2, lossItem);

    // 总输出数据量列
    auto *sizeItem = new QTableWidgetItem(QString("%1 ").arg(formatBytes(config.storageSize)));
    sizeItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(row, 3, sizeItem);

    // 码率列
    auto *rateItem = new QTableWidgetItem(QString("%1 kb/s").arg(config.bitRate));
    rateItem->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(row, 4, rateItem);
}

std::string StatisticsDialog::formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];
    return ss.str();
}

void StatisticsDialog::clear()
{
    systemMonitor_ = nullptr;
    networkMonitor_ = nullptr;
    vEncoder_ = nullptr;
}
