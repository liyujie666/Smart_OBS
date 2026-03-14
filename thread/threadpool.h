#ifndef THREADPOOL_H
#define THREADPOOL_H
#include "vsourcetaskthread.h"
#include "asourcetaskthread.h"
#include "mediasourcetaskthread.h"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>

using TaskKey = std::pair<int, int>;

namespace std {
template <>
struct hash<TaskKey> {
    size_t operator()(const TaskKey& key) const {
        // 组合两个 int 的哈希值，避免碰撞
        size_t hash1 = hash<int>()(key.first);  // sceneId 的哈希
        size_t hash2 = hash<int>()(key.second); // sourceId 的哈希
        return hash1 ^ (hash2 << 1);
    }
};
}

struct VThreadTask{
    int sceneId = -1;
    VSourceTaskThread* vThread = nullptr;
};

struct AThreadTask{
    int sceneId = -1;
    ASourceTaskThread* aThread = nullptr;
};

struct MediaThreadTask{
    int sceneId = -1;
    MediaSourceTaskThread* mediaThread = nullptr;
};


class ThreadPool: public QObject
{
    Q_OBJECT
public:
    ThreadPool(QObject* parent = nullptr);
    ~ThreadPool();

    // 添加任务：参数含 sceneId 和 sourceId，用于构造 TaskKey
    void addVideoTask(int sceneId,int sourceId,VSourceTaskThread* vThread);
    void addAudioTask(int sceneId,int sourceId,ASourceTaskThread* aThread);
    void addMediaTask(int sceneId,int sourceId, MediaSourceTaskThread* mediaThread);

    // 启动/停止任务（按场景筛选）
    void startVTasks(int sceneId = -1);
    void startATasks(int sceneId = -1);
    void stopVTasks(int sceneId = -1);
    void stopATasks(int sceneId = -1);

    // 移除任务：需传入 targetSceneId + sourceId，构造 TaskKey 精准查找
    void removeVTask(int sourceId, int targetSceneId);
    void removeATask(int sourceId, int targetSceneId);
    void removeMediaTask(int sourceId, int targetSceneId);

    // 获取任务集合（按场景筛选）
    std::vector<ASourceTaskThread*> getATasks(int sceneId = -1) const;
    std::vector<VSourceTaskThread*> getVTasks(int sceneId = -1) const;

    // 按 ID 获取任务：需传入 sceneId + sourceId，构造 TaskKey
    VSourceTaskThread* getVTaskById(int sceneId,int sourceId) const;
    ASourceTaskThread* getATaskById(int sceneId,int sourceId) const;
    std::unordered_map<int, MediaSourceTaskThread*> getMediaTasks(int sceneId = -1) const;

    // 控制帧处理（按场景）
    void startAddAudioFrame(int sceneId = -1);
    void stopAddAudioFrame(int sceneId = -1);

    // 设置视频帧率（按场景）
    void setVFps(int fps,int sceneId = -1);

private:
    // TaskKey（sceneId + sourceId）
    std::unordered_map<TaskKey, VThreadTask> vTasks_;
    std::unordered_map<TaskKey, AThreadTask> aTasks_;
    std::unordered_map<TaskKey, MediaThreadTask> mediaTasks_;

    mutable std::mutex m_mutex;
};

#endif // THREADPOOL_H
