#include "systemmonitor.h"
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <iomanip>

SystemMonitor::SystemMonitor() {
    last_network_check_ = std::chrono::system_clock::now();

#ifdef _WIN32
    // 初始化Windows特定数据
    win32_data_.process_handle_ = GetCurrentProcess();

    // 获取进程时间
    GetProcessTimes(win32_data_.process_handle_,
                    &win32_data_.createTime_,
                    &win32_data_.exitTime_,
                    &win32_data_.preKernelTime_,
                    &win32_data_.preUserTime_);

    // 获取系统时间
    GetSystemTimes(&win32_data_.preSysIdleTime_,
                   &win32_data_.preSysKernelTime_,
                   &win32_data_.preSysUserTime_);

    // 获取CPU核心数
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    win32_data_.cpuCoreCount_ = sysInfo.dwNumberOfProcessors;

    diskMonitorPath_ = "C:"; // 默认监测C盘
#else
    diskMonitorPath_ = "/";  // 默认监测根目录
#endif
}

SystemMonitor::~SystemMonitor() {
    stop();
}

void SystemMonitor::start(uint32_t interval, LoadCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) return;

    interval_ = interval;
    load_callback_ = callback;
    running_ = true;
    monitor_thread_ = std::thread(&SystemMonitor::monitorLoop, this);
}

void SystemMonitor::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        running_ = false;
    }

    cv_.notify_one();
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
}

void SystemMonitor::setThresholds(float high_cpu, float high_memory, uint64_t high_network_mbps) {
    std::lock_guard<std::mutex> lock(mutex_);
    high_cpu_threshold_ = high_cpu;
    high_memory_threshold_ = high_memory;
    high_network_threshold_ = high_network_mbps * 1024 * 1024 / 8;
}

SystemMetrics SystemMonitor::getCurrentMetrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_metrics_;
}

void SystemMonitor::setMonitoredInterfaces(const std::vector<std::string>& interfaces) {
    std::lock_guard<std::mutex> lock(mutex_);
    monitored_interfaces_ = interfaces;
}

void SystemMonitor::setDiskMonitorPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    diskMonitorPath_ = path;
}

void SystemMonitor::monitorLoop() {
    while (running_) {
        SystemMetrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();

        // 1. CPU指标
        metrics.cpu_usage = getCpuUsage();
        metrics.process_cpu_usage = getProcessCpuUsage();

        // 2. 内存指标
        getMemoryInfo(metrics.total_memory, metrics.used_memory, metrics.memory_usage);
        metrics.process_memory = getProcessMemoryUsage();

        // 3. 磁盘指标
        getDiskInfo(metrics.total_disk, metrics.free_disk, metrics.used_disk, metrics.disk_usage);

        // 4. 网络指标
        uint64_t current_sent, current_recv;
        getNetworkInfo(current_sent, current_recv);
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_network_check_).count();
        if (duration > 0 && last_sent_bytes_ > 0 && last_recv_bytes_ > 0) {
            metrics.sent_speed = (current_sent - last_sent_bytes_) / duration;
            metrics.recv_speed = (current_recv - last_recv_bytes_) / duration;
        }
        metrics.sent_bytes = current_sent;
        metrics.recv_bytes = current_recv;
        last_sent_bytes_ = current_sent;
        last_recv_bytes_ = current_recv;
        last_network_check_ = now;

        // 更新当前指标
        {
            std::lock_guard<std::mutex> lock(mutex_);
            current_metrics_ = metrics;
        }

        // 回调通知
        if (load_callback_) {
            load_callback_(calculateLoadStatus(metrics), metrics);
        }

        // 等待下一轮
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(interval_), [this]() { return !running_; });
    }
}

LoadStatus SystemMonitor::calculateLoadStatus(const SystemMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool is_high = (metrics.cpu_usage > high_cpu_threshold_) ||
                   (metrics.memory_usage > high_memory_threshold_) ||
                   (metrics.sent_speed > high_network_threshold_) ||
                   (metrics.disk_usage > 90.0f);

    bool is_low = (metrics.cpu_usage < high_cpu_threshold_ * 0.3) &&
                  (metrics.memory_usage < high_memory_threshold_ * 0.3) &&
                  (metrics.sent_speed < high_network_threshold_ * 0.3) &&
                  (metrics.disk_usage < 50.0f);

    return is_high ? LoadStatus::HIGH : (is_low ? LoadStatus::LOW : LoadStatus::NORMAL);
}

// FILETIME转100纳秒
_int64 SystemMonitor::fileTimeToInt64(const FILETIME* ftime) {
    LARGE_INTEGER li;
    li.LowPart = ftime->dwLowDateTime;
    li.HighPart = ftime->dwHighDateTime;
    return li.QuadPart; // FILETIME的QuadPart本身就是100纳秒为单位
}

// 进程CPU使用率获取（使用CpuMonitor逻辑）
float SystemMonitor::getProcessCpuUsage() {
#ifdef _WIN32
    FILETIME currKernelTime, currUserTime;
    FILETIME createTime, exitTime;

    // 获取当前进程的CPU时间
    if (!GetProcessTimes(win32_data_.process_handle_, &createTime, &exitTime, &currKernelTime, &currUserTime)) {
        std::cerr << "[Windows] GetProcessTimes failed, error: " << GetLastError() << std::endl;
        return 0.0f;
    }

    // 计算进程时间差（内核+用户，单位：100纳秒）
    _int64 kernelDiff = fileTimeToInt64(&currKernelTime) - fileTimeToInt64(&win32_data_.preKernelTime_);
    _int64 userDiff = fileTimeToInt64(&currUserTime) - fileTimeToInt64(&win32_data_.preUserTime_);
    _int64 processTotalDiff = kernelDiff + userDiff;

    // 更新上次进程时间戳
    win32_data_.preKernelTime_ = currKernelTime;
    win32_data_.preUserTime_ = currUserTime;

    // 总可用CPU时间（1秒 × 核心数 × 1e7 100纳秒/秒）
    _int64 totalCpuTime = 10000000LL * win32_data_.cpuCoreCount_;
    if (totalCpuTime == 0 || processTotalDiff < 0) {
        return 0.0f;
    }

    // 进程CPU占用率（限制在0-100%，保留1位小数）
    double usage = (static_cast<double>(processTotalDiff) / totalCpuTime) * 100.0;
    if (usage > 100.0) {
        usage = 100.0;
    }
    usage = std::round(usage * 10) / 10.0; // 保留1位小数

    return static_cast<float>(usage);
#else
    // Linux实现保持不变
    std::stringstream ss;
    ss << "/proc/" << getpid() << "/stat";
    std::ifstream file(ss.str());
    if (!file.is_open()) {
        std::cerr << "[Linux] 打开/proc/" << getpid() << "/stat失败，error: " << errno << std::endl;
        return 0.0f;
    }

    std::string line;
    std::getline(file, line);
    file.close();
    std::istringstream iss(line);
    std::vector<std::string> parts;
    std::string part;
    while (iss >> part) parts.push_back(part);
    if (parts.size() < 17) {
        std::cerr << "[Linux] /proc/" << getpid() << "/stat字段不足17个，数据异常" << std::endl;
        return 0.0f;
    }

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) {
        std::cerr << "[Linux] 获取sysconf(_SC_CLK_TCK)失败，error: " << errno << std::endl;
        return 0.0f;
    }

    uint64_t utime = std::stoull(parts[13]);
    uint64_t stime = std::stoull(parts[14]);
    uint64_t curr_proc_total_sec = (utime + stime) / clk_tck;

    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        std::cerr << "[Linux] 打开/proc/stat失败，error: " << errno << std::endl;
        return 0.0f;
    }

    std::string stat_line;
    std::getline(stat_file, stat_line);
    stat_file.close();
    std::istringstream stat_iss(stat_line);
    std::vector<std::string> cpu_parts;
    while (stat_iss >> part) cpu_parts.push_back(part);
    if (cpu_parts.size() < 8) {
        std::cerr << "[Linux] /proc/stat字段不足8个，数据异常" << std::endl;
        return 0.0f;
    }

    uint64_t sys_total_clock = 0;
    for (size_t i = 1; i < cpu_parts.size() && i <= 8; ++i) {
        sys_total_clock += std::stoull(cpu_parts[i]);
    }
    uint64_t curr_sys_total_sec = sys_total_clock / clk_tck;

    if (linux_data_.last_proc_total_ == 0 && linux_data_.last_cpu_total_ == 0) {
        linux_data_.last_proc_total_ = curr_proc_total_sec;
        linux_data_.last_cpu_total_ = curr_sys_total_sec;
        return 0.0f;
    }

    uint64_t proc_diff_sec = curr_proc_total_sec - linux_data_.last_proc_total_;
    uint64_t sys_diff_sec = curr_sys_total_sec - linux_data_.last_cpu_total_;

    if (sys_diff_sec <= 0 || proc_diff_sec < 0) {
        std::cerr << "[Linux] 耗时差异常：sys_diff=" << sys_diff_sec << "秒, "
                  << "proc_diff=" << proc_diff_sec << "秒" << std::endl;
        linux_data_.last_proc_total_ = curr_proc_total_sec;
        linux_data_.last_cpu_total_ = curr_sys_total_sec;
        return 0.0f;
    }

    float cpu_usage = (static_cast<float>(proc_diff_sec) / sys_diff_sec) * 100.0f;
    cpu_usage = std::min(cpu_usage, 100.0f);
    cpu_usage = std::round(cpu_usage * 10) / 10.0f;

    linux_data_.last_proc_total_ = curr_proc_total_sec;
    linux_data_.last_cpu_total_ = curr_sys_total_sec;

    return cpu_usage;
#endif
}

// 系统总CPU使用率获取（使用CpuMonitor逻辑）
float SystemMonitor::getCpuUsage() {
#ifdef _WIN32
    FILETIME currSysIdleTime, currSysKernelTime, currSysUserTime;

    // 获取当前系统时间
    if (!GetSystemTimes(&currSysIdleTime, &currSysKernelTime, &currSysUserTime)) {
        std::cerr << "[Windows] GetSystemTimes failed, error: " << GetLastError() << std::endl;
        return 0.0f;
    }

    // 计算系统时间差（单位：100纳秒）
    _int64 currSysTotal = fileTimeToInt64(&currSysKernelTime) + fileTimeToInt64(&currSysUserTime);
    _int64 preSysTotal = fileTimeToInt64(&win32_data_.preSysKernelTime_) + fileTimeToInt64(&win32_data_.preSysUserTime_);
    _int64 sysTotalDiff = currSysTotal - preSysTotal;

    // 系统空闲时间差
    _int64 sysIdleDiff = fileTimeToInt64(&currSysIdleTime) - fileTimeToInt64(&win32_data_.preSysIdleTime_);

    // 更新上次系统时间戳
    win32_data_.preSysIdleTime_ = currSysIdleTime;
    win32_data_.preSysKernelTime_ = currSysKernelTime;
    win32_data_.preSysUserTime_ = currSysUserTime;

    // 异常处理
    if (sysTotalDiff <= 0 || sysIdleDiff < 0) {
        return 0.0f;
    }

    // 计算系统CPU占用率
    double usage = (static_cast<double>(sysTotalDiff - sysIdleDiff) / sysTotalDiff) * 100.0;
    if (usage > 100.0) {
        usage = 100.0;
    }
    usage = std::round(usage * 10) / 10.0; // 保留1位小数

    return static_cast<float>(usage);
#else
    // Linux实现保持不变
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        std::cerr << "[Linux] 打开/proc/stat失败，error: " << errno << std::endl;
        return 0.0f;
    }

    std::string line;
    std::getline(file, line);
    file.close();
    if (line.substr(0, 3) != "cpu") {
        std::cerr << "[Linux] /proc/stat首行不是cpu，数据异常" << std::endl;
        return 0.0f;
    }

    std::istringstream iss(line);
    std::string cpu;
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t idle_total = idle + iowait;
    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) {
        std::cerr << "[Linux] 获取sysconf(_SC_CLK_TCK)失败，error: " << errno << std::endl;
        return 0.0f;
    }

    if (linux_data_.last_cpu_total_ == 0) {
        linux_data_.last_cpu_total_ = total;
        linux_data_.last_cpu_idle_ = idle_total;
        return 0.0f;
    }

    uint64_t total_diff = total - linux_data_.last_cpu_total_;
    uint64_t idle_diff = idle_total - linux_data_.last_cpu_idle_;

    if (total_diff == 0) {
        std::cerr << "[Linux] 系统CPU耗时差为0，返回0" << std::endl;
        return 0.0f;
    }

    float cpu_usage = 100.0f - (static_cast<float>(idle_diff) / total_diff) * 100.0f;
    cpu_usage = std::min(cpu_usage, 100.0f);
    cpu_usage = std::round(cpu_usage * 10) / 10.0f;

    linux_data_.last_cpu_total_ = total;
    linux_data_.last_cpu_idle_ = idle_total;

    return cpu_usage;
#endif
}

// 以下为其他函数实现（保持不变）
void SystemMonitor::getMemoryInfo(uint64_t& total, uint64_t& used, float& usage) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    total = memInfo.ullTotalPhys;
    used = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
    usage = static_cast<float>(memInfo.dwMemoryLoad);
#else
    struct sysinfo info;
    sysinfo(&info);
    total = static_cast<uint64_t>(info.totalram) * info.mem_unit;
    uint64_t free_mem = static_cast<uint64_t>(info.freeram + info.bufferram + info.sharedram) * info.mem_unit;
    used = total - free_mem;
    usage = static_cast<float>(used) / total * 100.0f;
#endif
}

uint64_t SystemMonitor::getProcessMemoryUsage() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(win32_data_.process_handle_, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
#else
    std::stringstream ss;
    ss << "/proc/" << getpid() << "/statm";
    std::ifstream file(ss.str());
    if (!file.is_open()) {
        std::cerr << "[Linux] 打开/proc/" << getpid() << "/statm失败，error: " << errno << std::endl;
        return 0;
    }
    unsigned long resident;
    file >> resident;
    return resident * sysconf(_SC_PAGESIZE);
#endif
}

// -------------------------- 修复：Windows 网络信息获取（改用 MIB_IFTABLE/MIB_IFROW） --------------------------
void SystemMonitor::getNetworkInfo(uint64_t& sent, uint64_t& recv) {
    sent = 0;
    recv = 0;

#ifdef _WIN32
    // 1. 先获取接口表所需的缓冲区大小
    ULONG out_buf_len = sizeof(MIB_IFTABLE);
    std::vector<BYTE> buf(out_buf_len);
    MIB_IFTABLE* if_table = reinterpret_cast<MIB_IFTABLE*>(buf.data());

    // 第一次调用：获取实际需要的缓冲区大小
    DWORD ret = GetIfTable(if_table, &out_buf_len, FALSE);
    if (ret == ERROR_INSUFFICIENT_BUFFER) {
        // 重新分配足够大的缓冲区
        buf.resize(out_buf_len);
        if_table = reinterpret_cast<MIB_IFTABLE*>(buf.data());
        // 第二次调用：真正获取接口表数据
        ret = GetIfTable(if_table, &out_buf_len, FALSE);
    }

    // 检查是否获取成功
    if (ret != NO_ERROR) {
        std::cerr << "[Windows] GetIfTable 失败，错误码：" << ret << std::endl;
        return;
    }

    // 2. 遍历所有网络接口
    for (DWORD i = 0; i < if_table->dwNumEntries; ++i) {
        MIB_IFROW& if_row = if_table->table[i];

        // 过滤条件1：只保留已启用的接口（管理员状态为“UP”）
        if (if_row.dwAdminStatus != MIB_IF_ADMIN_STATUS_UP) {
            continue;
        }

        // 过滤条件2：如果指定了监测接口列表，只保留列表中的接口
        if (!monitored_interfaces_.empty()) {
            // 将接口描述（bDescr）转换为 std::string（bDescr 是多字节字符串）
            std::string if_name(reinterpret_cast<char*>(if_row.bDescr));
            // 检查当前接口是否在监测列表中
            if (std::find(monitored_interfaces_.begin(), monitored_interfaces_.end(), if_name) == monitored_interfaces_.end()) {
                continue;
            }
        }

        // 累加发送/接收字节数（dwOutOctets=发送，dwInOctets=接收）
        sent += if_row.dwOutOctets;
        recv += if_row.dwInOctets;
    }
#else
    // Linux 网络信息获取逻辑不变（之前的代码正确）
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        std::cerr << "[Linux] getifaddrs 失败，错误码：" << errno << std::endl;
        return;
    }

    for (struct ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
        // 过滤非网络接口（无地址或非数据包类型）
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_PACKET) {
            continue;
        }

        std::string if_name(ifa->ifa_name);
        // 过滤不在监测列表中的接口
        if (!monitored_interfaces_.empty() &&
            std::find(monitored_interfaces_.begin(), monitored_interfaces_.end(), if_name) == monitored_interfaces_.end()) {
            continue;
        }

        // 获取接口统计信息（发送/接收字节数）
        struct rtnl_link_stats* stats = reinterpret_cast<struct rtnl_link_stats*>(ifa->ifa_data);
        sent += stats->tx_bytes;
        recv += stats->rx_bytes;
    }

    freeifaddrs(ifap); // 释放资源
#endif
}

void SystemMonitor::getDiskInfo(uint64_t& total, uint64_t& free, uint64_t& used, float& usage) {
    total = 0;
    free = 0;
    used = 0;
    usage = 0.0f;
#ifdef _WIN32
    std::wstring wpath(diskMonitorPath_.begin(), diskMonitorPath_.end());
    ULARGE_INTEGER totalBytes, freeBytesAvailable, totalFreeBytes;
    if (GetDiskFreeSpaceExW(wpath.c_str(), &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        total = totalBytes.QuadPart;
        free = freeBytesAvailable.QuadPart;
        used = total - totalFreeBytes.QuadPart;
        usage = total > 0 ? static_cast<float>(used) / total * 100.0f : 0.0f;
    }
#else
    struct statvfs stat;
    if (statvfs(diskMonitorPath_.c_str(), &stat) == 0) {
        total = static_cast<uint64_t>(stat.f_blocks) * stat.f_frsize;
        free = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
        used = total - static_cast<uint64_t>(stat.f_bfree) * stat.f_frsize;
        usage = total > 0 ? static_cast<float>(used) / total * 100.0f : 0.0f;
    }
#endif
}
