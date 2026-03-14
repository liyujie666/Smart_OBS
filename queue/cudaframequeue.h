#ifndef CUDAFRAMEQUEUE_H
#define CUDAFRAMEQUEUE_H

#include <infotools.h>
#include <queue>
#include <mutex>
#include <condition_variable>

class CudaFrameQueue
{
public:
    CudaFrameQueue(size_t maxSize = 30);
    CudaFrameQueue(const CudaFrameQueue&) = delete;
    CudaFrameQueue& operator=(const CudaFrameQueue&) = delete;
    ~CudaFrameQueue();

    void push(const CudaFrameInfo& frame);
    bool push(const CudaFrameInfo& frame, int timeoutMs);
    bool pop(CudaFrameInfo& outFrame);
    bool pop(CudaFrameInfo& outFrame, int timeoutMs);
    bool peek(CudaFrameInfo& outFrame) const;

    std::vector<CudaFrameInfo> peekAll() const;
    size_t removeOlderThan(int64_t thresholdTs);
    void forEach(const std::function<void(const CudaFrameInfo&)>& callback) const;
    bool isEmpty() const;
    size_t size() const;
    void clear();
    void stop();
    void reset();


private:
    mutable std::mutex mtx_;                  // 互斥锁（mutable允许const方法使用）
    std::condition_variable condPop_;         // 出队条件变量（等待队列非空）
    std::condition_variable condPush_;        // 入队条件变量（等待队列有空间）
    std::deque<CudaFrameInfo> deque_;         // 改用deque存储，支持迭代器
    size_t maxSize_;                          // 最大队列长度（防止内存溢出）
    bool stopped_ = false;

};

#endif // CUDAFRAMEQUEUE_H
