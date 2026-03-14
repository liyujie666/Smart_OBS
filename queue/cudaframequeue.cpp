#include "cudaframequeue.h"

CudaFrameQueue::CudaFrameQueue(size_t maxSize):maxSize_(maxSize)
{

}

CudaFrameQueue::~CudaFrameQueue()
{
    clear();
}

// 入队操作（阻塞直到队列有空间）
void CudaFrameQueue::push(const CudaFrameInfo& frame) {
    std::unique_lock<std::mutex> lock(mtx_);
    // 等待队列有空间（超过最大长度时阻塞）
    condPush_.wait(lock, [this]() { return deque_.size() < maxSize_; });
    deque_.push_back(frame);
    // 通知出队线程有新帧
    condPop_.notify_one();
}

// 入队操作（带超时，超时返回false）
bool CudaFrameQueue::push(const CudaFrameInfo& frame, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mtx_);
    // 等待队列有空间（超时返回失败）
    if (!condPush_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this]() { return deque_.size() < maxSize_; })) {
        return false; // 超时未入队
    }
    deque_.push_back(frame);
    condPop_.notify_one();
    return true;
}

// 出队操作（阻塞直到队列非空）
bool CudaFrameQueue::pop(CudaFrameInfo& outFrame) {
    std::unique_lock<std::mutex> lock(mtx_);
    // 等待队列非空
    condPop_.wait(lock, [this]() { return !deque_.empty() || stopped_; });
    if (stopped_) return false; // 已停止
    outFrame = deque_.front();
    deque_.pop_front();
    // 通知入队线程有空间
    condPush_.notify_one();
    return true;
}

// 出队操作（带超时，超时返回false）
bool CudaFrameQueue::pop(CudaFrameInfo& outFrame, int timeoutMs) {
    std::unique_lock<std::mutex> lock(mtx_);
    // 等待队列非空或超时
    if (!condPop_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                           [this]() { return !deque_.empty() || stopped_; })) {
        return false; // 超时未获取帧
    }
    if (stopped_) return false; // 已停止
    outFrame = deque_.front();
    deque_.pop_front();
    condPush_.notify_one();
    return true;
}

// 预览队头帧（不弹出，返回是否成功）
bool CudaFrameQueue::peek(CudaFrameInfo& outFrame) const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (deque_.empty()) return false;
    outFrame = deque_.front();
    return true;
}

// 预览所有帧
std::vector<CudaFrameInfo> CudaFrameQueue::peekAll() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return std::vector<CudaFrameInfo>(deque_.begin(), deque_.end());
}

// 遍历队列并执行回调）
void CudaFrameQueue::forEach(const std::function<void(const CudaFrameInfo&)>& callback) const {
    std::lock_guard<std::mutex> lock(mtx_);
    for (const auto& frame : deque_) {
        callback(frame); // 逐个处理帧，无需全量拷贝
    }
}

// 移除所有时间戳小于等于threshold的帧
size_t CudaFrameQueue::removeOlderThan(int64_t thresholdTs) {
    std::unique_lock<std::mutex> lock(mtx_);
    // 找到第一个ts > thresholdTs的位置
    auto it = std::find_if(deque_.begin(), deque_.end(),
                           [thresholdTs](const CudaFrameInfo& frame) {
                               return frame.timestamp > thresholdTs;
                           });
    // 计算要删除的元素数量
    size_t removed = std::distance(deque_.begin(), it);
    // 批量删除[begin, it)范围内的元素
    deque_.erase(deque_.begin(), it);
    // 通知入队线程可能有空间了
    if (removed > 0) {
        condPush_.notify_all();
    }
    return removed;
}

// 清空队列
void CudaFrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    deque_.clear();
    condPush_.notify_all(); // 通知所有等待入队的线程
}

// 获取当前队列大小
size_t CudaFrameQueue::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return deque_.size();
}

// 检查队列是否为空
bool CudaFrameQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return deque_.empty();
}

// 停止队列（唤醒所有阻塞的线程）
void CudaFrameQueue::stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    stopped_ = true;
    condPop_.notify_all(); // 唤醒所有等待出队的线程
    condPush_.notify_all(); // 唤醒所有等待入队的线程
}

// 重置队列（停止后可重新启用）
void CudaFrameQueue::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    stopped_ = false;
    clear();
}
