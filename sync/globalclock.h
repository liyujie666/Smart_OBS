#ifndef GLOBALCLOCK_H
#define GLOBALCLOCK_H

#include <chrono>
class GlobalClock
{
public:
    static GlobalClock& getInstance()
    {
        static GlobalClock instance;
        return instance;
    }

    int64_t getCurrentUs()
    {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

    GlobalClock(const GlobalClock&) = delete;
    GlobalClock& operator=(const GlobalClock&) = delete;

private:
    GlobalClock() = default;

};

#endif // GLOBALCLOCK_H
