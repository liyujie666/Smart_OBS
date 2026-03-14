#ifndef CPUMONITOR_H
#define CPUMONITOR_H

#include <QObject>
#include <windows.h>
#include <thread>
#include <chrono>
#include <QDebug>
#include <atomic>

class CpuMonitor : public QObject
{
    Q_OBJECT

public:
    explicit CpuMonitor(QObject* parent = nullptr) : QObject(parent)
    {
        hProcess_ = GetCurrentProcess();
        GetProcessTimes(hProcess_, &createTime_, &exitTime_, &preKernelTime_, &preUserTime_);

        // 获取CPU核心数（进程/系统CPU计算均需用到）
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        cpuCoreCount_ = sysInfo.dwNumberOfProcessors;
        qDebug() << "CPU核心数：" << cpuCoreCount_;

        // 获取首次系统时间（空闲时间、内核时间、用户时间）
        GetSystemTimes(&preSysIdleTime_, &preSysKernelTime_, &preSysUserTime_);

        isRunning_ = true;
    }

    ~CpuMonitor()
    {
        CloseHandle(hProcess_);
        stopMonitoring(); // 确保线程退出
    }

signals:

    void processCpuUsageUpdated(double usage);
    void systemCpuUsageUpdated(double usage);

public slots:
    // 启动监测（同时计算进程CPU和系统CPU）
    void startMonitoring()
    {
        while (isRunning_)
        {
            // 采样间隔：1秒（与你的原有逻辑一致）
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            calculateProcessCpuUsage();
            calculateSystemCpuUsage();
        }
    }

    // 停止监测（线程安全）
    void stopMonitoring()
    {
        isRunning_ = false;
    }

private:

    void calculateProcessCpuUsage()
    {
        FILETIME createTime, exitTime, currKernelTime, currUserTime;
        if (!GetProcessTimes(hProcess_, &createTime, &exitTime, &currKernelTime, &currUserTime))
        {
            emit processCpuUsageUpdated(0.0);
            return;
        }

        // 计算进程时间差（内核+用户，单位：100纳秒）
        _int64 kernelDiff = fileTimetoInt64(&currKernelTime) - fileTimetoInt64(&preKernelTime_);
        _int64 userDiff = fileTimetoInt64(&currUserTime) - fileTimetoInt64(&preUserTime_);
        _int64 processTotalDiff = kernelDiff + userDiff;

        // 更新上次进程时间戳
        preKernelTime_ = currKernelTime;
        preUserTime_ = currUserTime;

        // 总可用CPU时间（1秒 × 核心数 × 1e7 100纳秒/秒，与你的逻辑完全一致）
        _int64 totalCpuTime = 10000000LL * cpuCoreCount_; // 1秒=1e7个100纳秒
        if (totalCpuTime == 0 || processTotalDiff < 0)
        {
            emit processCpuUsageUpdated(0.0);
            return;
        }

        // 进程CPU占用率（限制在0-100%，保留1位小数）
        double usage = (static_cast<double>(processTotalDiff) / totalCpuTime) * 100.0;
        usage = qMin(usage, 100.0);
        usage = qRound(usage * 10) / 10.0; // 保留1位小数

        emit processCpuUsageUpdated(usage);
    }

    void calculateSystemCpuUsage()
    {
        FILETIME currSysIdleTime, currSysKernelTime, currSysUserTime;
        // 获取当前系统时间：空闲时间、内核时间、用户时间（Windows API原生支持）
        if (!GetSystemTimes(&currSysIdleTime, &currSysKernelTime, &currSysUserTime))
        {
            emit systemCpuUsageUpdated(0.0);
            return;
        }

        // 1. 计算时间差（单位：100纳秒）
        // 系统总CPU时间差 = （当前内核时间 + 当前用户时间） - （上次内核时间 + 上次用户时间）
        _int64 currSysTotal = fileTimetoInt64(&currSysKernelTime) + fileTimetoInt64(&currSysUserTime);
        _int64 preSysTotal = fileTimetoInt64(&preSysKernelTime_) + fileTimetoInt64(&preSysUserTime_);
        _int64 sysTotalDiff = currSysTotal - preSysTotal;

        // 系统空闲时间差 = 当前空闲时间 - 上次空闲时间
        _int64 sysIdleDiff = fileTimetoInt64(&currSysIdleTime) - fileTimetoInt64(&preSysIdleTime_);

        // 2. 更新上次系统时间戳
        preSysIdleTime_ = currSysIdleTime;
        preSysKernelTime_ = currSysKernelTime;
        preSysUserTime_ = currSysUserTime;

        // 3. 异常处理（时间差为负或0，返回0%）
        if (sysTotalDiff <= 0 || sysIdleDiff < 0)
        {
            emit systemCpuUsageUpdated(0.0);
            return;
        }

        // 4. 系统总CPU占用率 = （总时间差 - 空闲时间差） / 总时间差 × 100%
        // （总时间差-空闲时间差）= 系统实际使用的CPU时间
        double usage = (static_cast<double>(sysTotalDiff - sysIdleDiff) / sysTotalDiff) * 100.0;
        usage = qMin(usage, 100.0);       // 限制最大100%
        usage = qRound(usage * 10) / 10.0; // 保留1位小数（与进程CPU格式一致）

        emit systemCpuUsageUpdated(usage);
    }

    // FILETIME转100纳秒
    _int64 fileTimetoInt64(const FILETIME* ftime)
    {
        LARGE_INTEGER li;
        li.LowPart = ftime->dwLowDateTime;
        li.HighPart = ftime->dwHighDateTime;
        return li.QuadPart; // 直接返回100纳秒单位（你的原有注释有误，无需除以10）
        // 注：FILETIME的QuadPart本身就是100纳秒为单位，之前除以10会导致单位错误，已修正
    }

private:
    // 进程CPU相关成员
    HANDLE hProcess_;                       // 当前进程句柄
    FILETIME createTime_, exitTime_;        // 进程创建/退出时间
    FILETIME preKernelTime_, preUserTime_;  // 上次进程内核/用户时间

    // 系统CPU
    FILETIME preSysIdleTime_;               // 上次系统空闲时间
    FILETIME preSysKernelTime_;             // 上次系统内核时间
    FILETIME preSysUserTime_;               // 上次系统用户时间

    int cpuCoreCount_;                      // CPU核心数（进程/系统计算均需）
    std::atomic<bool> isRunning_;           // 监测线程控制（原子变量，线程安全）
};

#endif // CPUMONITOR_H
