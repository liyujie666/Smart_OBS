#include "queue/packetqueue.h"
#include "pool/gloabalpool.h"
#include <QDebug>
extern "C" {
#include <libavcodec/avcodec.h>
}

PacketQueue::~PacketQueue() {
    clear();
}

// void PacketQueue::push(AVPacket* pkt) {
//     if (!pkt) return;
//     AVPacket* pkt_copy = GlobalPool::getPacketPool().get();
//     if (!pkt_copy) return;

//     if (av_packet_ref(pkt_copy, pkt) < 0) {
//         GlobalPool::getPacketPool().recycle(pkt_copy);
//         return;
//     }

//     {
//         std::unique_lock<std::mutex> lock(m_mutex);
//         // 队列满了，阻塞等待或你也可以直接return丢弃
//         m_cond.wait(lock, [this]() { return m_queue.size() < maxPackets_ || isInterrupted_; });
//         m_queue.push(pkt_copy);
//     }
//     // qDebug() << "pkt size" << size();
//     m_cond.notify_one();
// }

void PacketQueue::push(AVPacket* pkt) {
    if (!pkt) return;

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // 等待队列有空间（维持原有阻塞逻辑）
        m_cond.wait(lock, [this]() { return m_queue.size() < maxPackets_ || isInterrupted_; });
        m_queue.push(pkt); // 直接推送原始包，不复制
    }
    m_cond.notify_one();
}

AVPacket* PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    // 等待队列非空
    m_cond.wait(lock, [this] {
        return !m_queue.empty() || isInterrupted_;
    });

    if (isInterrupted_ && m_queue.empty()) {
        return nullptr;  // 表示队列关闭，无需再等
    }

    AVPacket* pkt = m_queue.front();
    m_queue.pop();
    m_cond.notify_one();
    return pkt;
}
void PacketQueue::close()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    isClosed_ = true;
    m_cond.notify_all();
}

bool PacketQueue::isEmpty() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();

}

void PacketQueue::interrupt() {
    std::lock_guard<std::mutex> lock(m_mutex);
    isInterrupted_ = true;
    m_cond.notify_all(); // 唤醒等待的线程
}

void PacketQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        AVPacket* pkt = m_queue.front();
        m_queue.pop();
        GlobalPool::getPacketPool().recycle(pkt);
    }
    isClosed_ = false;
}

int PacketQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (int)m_queue.size();
}
