#include "threadpool.h"
#include <QDebug>

ThreadPool::ThreadPool(QObject* parent) : QObject(parent){}

ThreadPool::~ThreadPool()
{
    stopVTasks();
    stopATasks();
    // 清理媒体任务：遍历 TaskKey 键的 map
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : mediaTasks_) {
        if(pair.second.mediaThread)
        {
            pair.second.mediaThread->stop();
            delete pair.second.mediaThread;
            pair.second.mediaThread = nullptr;
        }
    }
    mediaTasks_.clear();
}

void ThreadPool::addVideoTask(int sceneId, int sourceId, VSourceTaskThread *vThread)
{
    if(!vThread || sceneId < 0 || sourceId < 0) return; // 增加 sourceId 有效性校验

    std::lock_guard<std::mutex> locker(m_mutex);
    // 1. 构造 TaskKey（sceneId + sourceId）
    TaskKey key = {sceneId, sourceId};
    // 2. 查找该组合键是否已存在（避免重复添加导致内存泄漏）
    auto it = vTasks_.find(key);
    if (it != vTasks_.end() && it->second.vThread) {
        it->second.vThread->stop();
        delete it->second.vThread;
        qWarning() << "sceneId=" << sceneId << ", sourceId=" << sourceId << "已存在，替换旧线程";
    }
    // 3. 插入新线程（键为 TaskKey）
    vTasks_[key] = {sceneId, vThread};
}

void ThreadPool::addAudioTask(int sceneId, int sourceId, ASourceTaskThread *aThread)
{
    if(!aThread || sceneId < 0 || sourceId < 0) return;

    std::lock_guard<std::mutex> locker(m_mutex);
    TaskKey key = {sceneId, sourceId};
    auto it = aTasks_.find(key);
    if (it != aTasks_.end() && it->second.aThread) {
        it->second.aThread->stop();
        delete it->second.aThread;
        qWarning() << "sceneId=" << sceneId << ", sourceId=" << sourceId << "已存在，替换旧线程";
    }
    aTasks_[key] = {sceneId, aThread};
}

void ThreadPool::addMediaTask(int sceneId, int sourceId, MediaSourceTaskThread* mediaThread) {
    if (!mediaThread || sceneId < 0 || sourceId < 0) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    TaskKey key = {sceneId, sourceId};
    auto it = mediaTasks_.find(key);
    if (it != mediaTasks_.end() && it->second.mediaThread) {
        it->second.mediaThread->stop();
        delete it->second.mediaThread;
        qWarning() << "sceneId=" << sceneId << ", sourceId=" << sourceId << "已存在，替换旧线程";
    }
    mediaTasks_[key] = {sceneId, mediaThread};
}

void ThreadPool::startVTasks(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for(auto& pair : vTasks_)
    {
        VThreadTask& task = pair.second; // 注意：用引用避免拷贝
        // 按场景筛选：sceneId=-1 启动所有，否则只启动目标场景
        if(sceneId != -1 && task.sceneId != sceneId){
            continue;
        }

        if(task.vThread && !task.vThread->isRunning())
        {
            task.vThread->start();
            qDebug() << "启动视频线程：sceneId=" << task.sceneId << ", sourceId=" << pair.first.second;
        }
    }
}

void ThreadPool::startATasks(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for(auto& pair : aTasks_)
    {
        AThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId){
            continue;
        }

        if(task.aThread && !task.aThread->isRunning())
        {
            task.aThread->start();
            qDebug() << "启动音频线程：sceneId=" << task.sceneId << ", sourceId=" << pair.first.second;
        }
    }
}

void ThreadPool::stopVTasks(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = vTasks_.begin();
    while (it != vTasks_.end()) {
        VThreadTask& task = it->second;
        // 按场景筛选：只停止目标场景的线程
        if(sceneId != -1 && task.sceneId != sceneId){
            ++it;
            continue;
        }

        if(task.vThread){
            task.vThread->stop();
            delete task.vThread;
            task.vThread = nullptr;
            qDebug() << "停止视频线程：sceneId=" << task.sceneId << ", sourceId=" << it->first.second;
        }

        it = vTasks_.erase(it); //  erase 后迭代器自动更新
    }
}

void ThreadPool::stopATasks(int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    auto it = aTasks_.begin();
    while(it != aTasks_.end())
    {
        AThreadTask& task = it->second;
        if(sceneId != -1 && task.sceneId != sceneId)
        {
            ++it;
            continue;
        }

        if(task.aThread)
        {
            task.aThread->stop();
            delete task.aThread;
            task.aThread = nullptr;
            qDebug() << "停止音频线程：sceneId=" << task.sceneId << ", sourceId=" << it->first.second;
        }
        it = aTasks_.erase(it);
    }
}

void ThreadPool::removeVTask(int sourceId, int targetSceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    // 构造 TaskKey：目标场景 + 源ID（精准查找，避免误删其他场景）
    TaskKey key = {targetSceneId, sourceId};
    auto it = vTasks_.find(key);
    if(it == vTasks_.end()) {
        qWarning() << "移除视频线程失败：sceneId=" << targetSceneId << ", sourceId=" << sourceId << "不存在";
        return;
    }

    VThreadTask& task = it->second;
    // 二次校验场景（防止键构造错误）
    if(task.sceneId != targetSceneId){
        qWarning() << "移除视频线程失败：sceneId不匹配（实际：" << task.sceneId << "，目标：" << targetSceneId << ")";
        return;
    }

    if(task.vThread)
    {
        task.vThread->stop();
        delete task.vThread;
        task.vThread = nullptr;
        qDebug() << "成功移除视频线程：sceneId=" << task.sceneId << ", sourceId=" << sourceId;
    }

    vTasks_.erase(it);
}

void ThreadPool::removeATask(int sourceId, int targetSceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    TaskKey key = {targetSceneId, sourceId};
    auto it = aTasks_.find(key);
    if (it == aTasks_.end()) {
        qWarning() << "移除音频线程失败：sceneId=" << targetSceneId << ", sourceId=" << sourceId << "不存在";
        return;
    }

    AThreadTask& task = it->second;
    if(task.sceneId != targetSceneId){
        qWarning() << "移除音频线程失败：sceneId不匹配（实际：" << task.sceneId << "，目标：" << targetSceneId << ")";
        return;
    }

    if(task.aThread)
    {
        task.aThread->stop();
        delete task.aThread;
        task.aThread = nullptr;
        qDebug() << "成功移除音频线程：sceneId=" << task.sceneId << ", sourceId=" << sourceId;
    }
    aTasks_.erase(it);
}

void ThreadPool::removeMediaTask(int sourceId, int targetSceneId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    TaskKey key = {targetSceneId, sourceId};
    auto it = mediaTasks_.find(key);
    if (it == mediaTasks_.end()) {
        qWarning() << "移除媒体线程失败：sceneId=" << targetSceneId << ", sourceId=" << sourceId << "不存在";
        return;
    }

    MediaThreadTask& task = it->second;
    if(task.sceneId != targetSceneId){
        qWarning() << "移除媒体线程失败：sceneId不匹配（实际：" << task.sceneId << "，目标：" << targetSceneId << ")";
        return;
    }

    if(task.mediaThread)
    {
        task.mediaThread->stop();
        delete task.mediaThread;
        task.mediaThread = nullptr;
        qDebug() << "成功移除媒体线程：sceneId=" << task.sceneId << ", sourceId=" << sourceId;
    }
    mediaTasks_.erase(it);
}

std::vector<ASourceTaskThread *> ThreadPool::getATasks(int sceneId) const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    std::vector<ASourceTaskThread*> result;
    for (const auto& pair : aTasks_) {
        const AThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId) {
            continue;
        }
        if(task.aThread){
            result.push_back(task.aThread);
        }
    }
    return result;
}

std::vector<VSourceTaskThread *> ThreadPool::getVTasks(int sceneId) const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    std::vector<VSourceTaskThread*> result;
    for (const auto& pair : vTasks_) {
        const VThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId){
            continue;
        }
        if(task.vThread){
            result.push_back(task.vThread);
        }
    }
    return result;
}

VSourceTaskThread* ThreadPool::getVTaskById(int sceneId, int sourceId) const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    TaskKey key = {sceneId, sourceId};
    auto it = vTasks_.find(key);
    if (it == vTasks_.end()) {
        qDebug() << "getVTaskById: sceneId=" << sceneId << ", sourceId=" << sourceId << "不存在";
        return nullptr;
    }
    const VThreadTask& task = it->second;
    if (task.sceneId != sceneId) {
        qDebug() << "getVTaskById: sceneId不匹配（实际：" << task.sceneId << "，目标：" << sceneId << ")";
        return nullptr;
    }
    return task.vThread;
}

ASourceTaskThread *ThreadPool::getATaskById(int sceneId, int sourceId) const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    TaskKey key = {sceneId, sourceId};
    auto it = aTasks_.find(key);
    if(it == aTasks_.end()) {
        qDebug() << "getATaskById: sceneId=" << sceneId << ", sourceId=" << sourceId << "不存在";
        return nullptr;
    }
    const AThreadTask& task = it->second;
    if(task.sceneId != sceneId){
        qDebug() << "getATaskById: sceneId不匹配（实际：" << task.sceneId << "，目标：" << sceneId << ")";
        return nullptr;
    }
    return task.aThread;
}

std::unordered_map<int, MediaSourceTaskThread*> ThreadPool::getMediaTasks(int sceneId) const
{
    std::lock_guard<std::mutex> locker(m_mutex);
    std::unordered_map<int, MediaSourceTaskThread*> mediaMap;
    for(const auto& pair : mediaTasks_)
    {
        const MediaThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId) continue;
        if(task.mediaThread){
            mediaMap[pair.first.first] = task.mediaThread;
        }
    }
    return mediaMap;
}

void ThreadPool::startAddAudioFrame(int sceneId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : aTasks_)
    {
        AThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId) continue;
        if(task.aThread){
            task.aThread->startAddingFrame();
        }
    }
}


void ThreadPool::stopAddAudioFrame(int sceneId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : aTasks_)
    {
        AThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId) continue;
        if(task.aThread){
            task.aThread->stopAddingFrame();
        }
    }
}


void ThreadPool::setVFps(int fps, int sceneId)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    for(auto& pair : vTasks_)
    {
        VThreadTask& task = pair.second;
        if(sceneId != -1 && task.sceneId != sceneId) continue;
        if(task.vThread && task.vThread->isRunning())
        {
            task.vThread->setFPS(fps);
            qDebug() << "设置视频帧率：sceneId=" << task.sceneId << ", sourceId=" << pair.first.second << ", fps=" << fps;
        }
    }
}
