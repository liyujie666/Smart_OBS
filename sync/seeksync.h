#ifndef SEEKSYNC_H
#define SEEKSYNC_H
#include <atomic>
#include <cstdint>

struct SeekSync{

    std::atomic<int> serial{0};
    std::atomic<bool> seeking{false};
    std::atomic<bool> aReady{false};
    std::atomic<bool> vReady{false};
    std::atomic<int64_t> targetUs{0};


};

#endif // SEEKSYNC_H
