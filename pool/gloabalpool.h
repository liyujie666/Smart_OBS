#ifndef GLOABALPOOL_H
#define GLOABALPOOL_H
#include "pool/framepool.h"
#include "pool/packetpool.h"

class GlobalPool {
public:
    static PacketPool& getPacketPool() {
        static PacketPool pool(128);
        return pool;
    }

    static FramePool& getFramePool() {
        static FramePool pool(128);
        return pool;
    }

    static void setFramePoolMaxSize(size_t newMaxSize) {
        return getFramePool().setMaxSize(newMaxSize);
    }

    static void setPacketPoolMaxSize(size_t newMaxSize) {
        return getPacketPool().setMaxSize(newMaxSize);
    }

    static size_t getFramePoolMaxSize() {
        return getFramePool().getMaxSize();
    }

    static size_t getPacketPoolMaxSize() {
        return getPacketPool().getMaxSize();
    }
};
#endif // GLOABALPOOL_H
