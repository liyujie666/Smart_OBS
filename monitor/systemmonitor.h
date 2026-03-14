#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#endif

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <string>
#include <atomic>
#include <map>
#include <iostream>

// 系统负载指标结构体
struct SystemMetrics {
    // CPU指标
    float cpu_usage = 0.0f;          // 总体CPU使用率(%)
    float process_cpu_usage = 0.0f;  // 当前进程CPU使用率(%)

    // 内存指标
    uint64_t total_memory = 0;       // 总内存(字节)
    uint64_t used_memory = 0;        // 已用内存(字节)
    float memory_usage = 0.0f;       // 内存使用率(%)
    uint64_t process_memory = 0;     // 进程内存(字节)

    // 磁盘指标
    uint64_t total_disk = 0;         // 磁盘总空间(字节)
    uint64_t free_disk = 0;          // 磁盘可用空间(字节)
    uint64_t used_disk = 0;          // 磁盘已用空间(字节)
    float disk_usage = 0.0f;         // 磁盘使用率(%)

    // 网络指标
    uint64_t sent_bytes = 0;         // 发送字节数
    uint64_t recv_bytes = 0;         // 接收字节数
    uint64_t sent_speed = 0;         // 发送速度(字节/秒)
    uint64_t recv_speed = 0;         // 接收速度(字节/秒)

    // 时间戳
    std::chrono::system_clock::time_point timestamp;
};

// 负载状态枚举
enum class LoadStatus {
    LOW,    // 低负载
    NORMAL, // 正常负载
    HIGH    // 高负载
};

// 系统监测器类
class SystemMonitor {
public:
    using LoadCallback = std::function<void(LoadStatus, const SystemMetrics&)>;

    SystemMonitor();
    ~SystemMonitor();

    void start(uint32_t interval = 1000, LoadCallback callback = nullptr);
    void stop();
    void setThresholds(float high_cpu = 80.0f, float high_memory = 85.0f,
                       uint64_t high_network_mbps = 50);
    SystemMetrics getCurrentMetrics();
    void setMonitoredInterfaces(const std::vector<std::string>& interfaces);
    void setDiskMonitorPath(const std::string& path);

private:
    void monitorLoop();
    float getCpuUsage();            // 系统总CPU使用率
    float getProcessCpuUsage();     // 当前进程CPU使用率
    void getMemoryInfo(uint64_t& total, uint64_t& used, float& usage);
    uint64_t getProcessMemoryUsage();
    void getNetworkInfo(uint64_t& sent, uint64_t& recv);
    void getDiskInfo(uint64_t& total, uint64_t& free, uint64_t& used, float& usage);
    LoadStatus calculateLoadStatus(const SystemMetrics& metrics);

    // FILETIME转100纳秒(内部使用)
    _int64 fileTimeToInt64(const FILETIME* ftime);

    // 线程控制变量
    std::atomic<bool> running_{false};
    std::thread monitor_thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    uint32_t interval_ = 1000;

    // 阈值设置
    float high_cpu_threshold_ = 80.0f;
    float high_memory_threshold_ = 85.0f;
    uint64_t high_network_threshold_ = 50 * 1024 * 1024 / 8; // 50Mbps转换为字节/秒

    // 当前系统指标
    SystemMetrics current_metrics_;

    // 网络统计缓存
    uint64_t last_sent_bytes_ = 0;
    uint64_t last_recv_bytes_ = 0;
    std::chrono::system_clock::time_point last_network_check_;

    // 磁盘监测路径
    std::string diskMonitorPath_;

    // 其他成员
    LoadCallback load_callback_;
    std::vector<std::string> monitored_interfaces_;

#ifdef _WIN32
    struct Win32Data {
        // 进程CPU相关
        HANDLE process_handle_ = nullptr;
        FILETIME preKernelTime_ = {0};  // 上次进程内核时间
        FILETIME preUserTime_ = {0};    // 上次进程用户时间
        FILETIME createTime_ = {0};     // 进程创建时间
        FILETIME exitTime_ = {0};       // 进程退出时间

        // 系统CPU相关
        FILETIME preSysIdleTime_ = {0};   // 上次系统空闲时间
        FILETIME preSysKernelTime_ = {0}; // 上次系统内核时间
        FILETIME preSysUserTime_ = {0};   // 上次系统用户时间

        int cpuCoreCount_ = 0;           // CPU核心数
    };
    Win32Data win32_data_;
#else
    struct LinuxData {
        uint64_t last_cpu_total_ = 0;       // 上一次系统总CPU时间
        uint64_t last_proc_total_ = 0;      // 上一次进程总CPU时间
        uint64_t last_cpu_idle_ = 0;        // 上一次系统空闲时间
        std::map<std::string, uint64_t> last_interface_sent_;
        std::map<std::string, uint64_t> last_interface_recv_;
    };
    LinuxData linux_data_;
#endif
};

#endif // SYSTEMMONITOR_H
