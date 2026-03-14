#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavcodec/avcodec.h>
}

class PacketQueue {
public:
    PacketQueue() = default;
    ~PacketQueue();

    void push(AVPacket* pkt);
    AVPacket* pop();
    void close();
    bool isEmpty() const;
    void interrupt();
    void clear();
    int size() const;


private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<AVPacket*> m_queue;
    int maxPackets_ = 100;
    bool isClosed_ = false;
    std::atomic<bool> isInterrupted_ = false;


};

#endif // PACKETQUEUE_H
