#ifndef SCOPEDTIMER_H
#define SCOPEDTIMER_H

#include <chrono>
#include <QString>
#include "BenchmarkCollector.h"

/**
 * @brief RAII 计时器，用于对代码块进行插桩
 *
 * 用法：
 *   {
 *       ScopedTimer timer("cuda_rgba_to_nv12");
 *       // ... 要计时的代码 ...
 *   } // 析构时自动记录耗时到 BenchmarkCollector
 *
 * 也可以手动结束：
 *   ScopedTimer timer("encode_frame");
 *   doEncode();
 *   timer.stop(); // 提前结束计时
 */
class ScopedTimer
{
public:
    explicit ScopedTimer(const QString& tag)
        : tag_(tag)
        , start_(std::chrono::high_resolution_clock::now())
        , stopped_(false)
    {}

    ~ScopedTimer() {
        stop();
    }

    void stop() {
        if (stopped_) return;
        stopped_ = true;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        BenchmarkCollector::instance().record(tag_, ms);
    }

    /// 获取当前已经过的时间（毫秒），不停止计时
    double elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

private:
    QString tag_;
    std::chrono::high_resolution_clock::time_point start_;
    bool stopped_;
};

/**
 * @brief 条件计时器，只在BenchmarkCollector 运行时才计时
 *
 * 用于高频调用路径（如每帧渲染），避免在非 benchmark 模式下的开销。
 */
class ConditionalScopedTimer
{
public:
    explicit ConditionalScopedTimer(const QString& tag)
        : tag_(tag)
        , active_(BenchmarkCollector::instance().isRunning())
    {
        if (active_) {
            start_ = std::chrono::high_resolution_clock::now();
        }
    }

    ~ConditionalScopedTimer() {
        if (!active_ || stopped_) return;
        stopped_ = true;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        BenchmarkCollector::instance().record(tag_, ms);
    }

    void stop() {
        if (!active_ || stopped_) return;
        stopped_ = true;
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start_).count();
        BenchmarkCollector::instance().record(tag_, ms);
    }

private:
    QString tag_;
    std::chrono::high_resolution_clock::time_point start_;
    bool active_ = false;
    bool stopped_ = false;
};

// 便捷宏：自动以函数名作为 tag
#define BENCHMARK_SCOPE() ScopedTimer _benchTimer_(QString(__FUNCTION__))
#define BENCHMARK_SCOPE_TAG(tag) ScopedTimer _benchTimer_(tag)
#define BENCHMARK_COND_SCOPE(tag) ConditionalScopedTimer _benchCondTimer_(tag)

#endif // SCOPEDTIMER_H
