#include "queue/framequeue.h"
#include "pool/gloabalpool.h"
#include <QDebug>
extern "C" {
#include <libavutil/frame.h>
}


FrameQueue::~FrameQueue() {
    clear();
}

void FrameQueue::push(AVFrame* frame) {
    if (!frame) return;

    AVFrame* frame_copy = GlobalPool::getFramePool().get();
    if (!frame_copy) return;
    if (av_frame_ref(frame_copy, frame) < 0) {
        GlobalPool::getFramePool().recycle(frame_copy);
        return;
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.push(frame_copy);
    }
    // qDebug() << "frame size" << size();
    m_cond.notify_one();
}




AVFrame* FrameQueue::pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    // 等待队列非空
    m_cond.wait(lock, [this] {
        bool wakeReason = !m_queue.empty() || isClosed_ || isInterrupted_;
        // qDebug() << "【FrameQueue::pop】等待状态：队列非空=" << !m_queue.empty()
        //          << "，isClosed_=" << isClosed_ << "，isInterrupted_=" << isInterrupted_
        //          << "，唤醒条件=" << wakeReason;
        return wakeReason;
    });

    if (isClosed_ || isInterrupted_ || m_queue.empty()) {
        return nullptr;
    }

    AVFrame* frame = m_queue.front();
    m_queue.pop();
    return frame;
}

void FrameQueue::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    isClosed_ = true;
    m_cond.notify_all();
}

bool FrameQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

bool FrameQueue::isClosed() const
{
    return isClosed_;
}
void FrameQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        AVFrame* frame = m_queue.front();
        m_queue.pop();
        GlobalPool::getFramePool().recycle(frame);
    }

}

int FrameQueue::size() const {
    // qDebug () << "ready into size";
    std::lock_guard<std::mutex> lock(m_mutex);
    // qDebug () << " size is get into";
    return (int)m_queue.size();
}

void FrameQueue::interrupt()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    isInterrupted_ = true;
    m_cond.notify_all(); // 唤醒等待的线程
}

