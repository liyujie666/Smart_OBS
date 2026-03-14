#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavutil/frame.h>
}

class FrameQueue {
public:
    FrameQueue() = default;
    ~FrameQueue();

    void push(AVFrame* frame);
    AVFrame* pop();
    bool isEmpty() const;
    bool isClosed() const;
    int size() const;
    void close();


    void interrupt();
    void clear();

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<AVFrame*> m_queue;
    bool isClosed_ = false;
    int m_maxSize = 50;
    std::atomic<bool> isInterrupted_ = false;
};

#endif // FRAMEQUEUE_H
