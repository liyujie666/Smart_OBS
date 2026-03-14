#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QTimer>
#include "monitor/systemmonitor.h"   // 系统监测头文件
#include "monitor/networkmonitor.h"  // 网络监测头文件

namespace Ui {
class StatisticsDialog;
}

enum class StatisticType{
    Record,
    Stream
};

struct GlobalConfig{
    float cpuUsage;             // CPU使用率 (%)
    float processCpuUsage;      // 当前进程CPU使用率
    int fps;                    // 当前帧率
    int expectFps;              // 设定的帧率
    uint64_t diskSpace;         // 磁盘剩余空间 (GB)
    float diakUsage;            // 磁盘使用率
    uint64_t processMemory;     // 内存占用 (MB)
    float memoryUsage;          // 内存使用率
    uint64_t totalEncodedFrames;    // 总编码的帧
    uint64_t delayEncodedFrames;    // 编码延迟的帧
    uint64_t totalRenderedFrames;   // 总渲染的帧
    uint64_t delayRenderedFrames;   // 渲染延迟的帧
};

struct SceneConfig{
    StatisticType type;
    bool isActive;          // 是否活跃（推流/录制中）
    int lossFrame;          // 丢帧数
    uint64_t totalFrames;   // 总帧数
    float lossRate;         // 丢包率
    uint64_t storageSize;   // 总输出数据量 (MiB)
    int bitRate;            // 码率 (kb/s)
};

class VEncoder;
class FPSCounter;
class StatisticsDialog : public QDialog
{
    Q_OBJECT

public:
    static StatisticsDialog* getInstance() {
        if (!s_instance) {
            s_instance = new StatisticsDialog;
        }
        return s_instance;
    }

    static void destroyInstance() {
        if (s_instance) {
            delete s_instance;
            s_instance = nullptr;
        }
    }

    StatisticsDialog(const StatisticsDialog&) = delete;
    StatisticsDialog& operator=(const StatisticsDialog&) = delete;

    void initTableWidget();
    // 设置系统/网络监测模块的引用，用于数据同步
    void setSystemMonitors(SystemMonitor* systemMonitor);
    void setNetWorkMonitors(NetworkMonitor* networkMonitor);
    void setEncodeConfig(VEncoder* vEncoder,FPSCounter* fpsCounter);
    void updateGlobalConfig(const GlobalConfig& config);
    void updateSceneConfig(StatisticType type, const SceneConfig& config);
    std::string formatBytes(uint64_t bytes);
    void clear();

public slots:
    // 定时刷新UI的槽函数
    void onRefreshTimerTimeout();
    // 推流状态实时更新槽函数（接收NetworkMonitor信号）
    void onStreamStatusUpdated(const NetworkMonitorResult& result);
private slots:

    void onUpdateFps(int fps);
private:
    explicit StatisticsDialog(QWidget *parent = nullptr);
    ~StatisticsDialog();
    Ui::StatisticsDialog *ui;

    static StatisticsDialog* s_instance;
    SystemMonitor* systemMonitor_ = nullptr;   // 系统监测模块
    NetworkMonitor* networkMonitor_ = nullptr; // 网络监测模块
    VEncoder* vEncoder_ = nullptr;
    QTimer refreshTimer_;                      // UI刷新定时器
    GlobalConfig globalConfig_;                // 全局配置数据
    SceneConfig recordConfig_;                 // 录制配置数据
    SceneConfig streamConfig_;                 // 推流配置数据
};

#endif // STATISTICSDIALOG_H
