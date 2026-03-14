// #ifndef MEDIASOURCECONTROLLER_H
// #define MEDIASOURCECONTROLLER_H

// #include <QObject>
// #include <memory>
// #include <mutex>
// #include "source/video/localvideosource.h"
// #include "source/audio/mediaaudiosource.h"
// #include "thread/mediasourcetaskthread.h"
// #include "component/audioitemwidget.h"
// #include "sync/mediaclock.h"

// using MediaSourceKey = std::pair<int, int>;


// namespace std {
// template <>
// struct hash<MediaSourceKey> {
//     size_t operator()(const MediaSourceKey& key) const {
//         // 组合两个 int 的哈希值，避免碰撞
//         size_t hash1 = hash<int>()(key.first);  // sceneId 的哈希
//         size_t hash2 = hash<int>()(key.second); // sourceId 的哈希
//         return hash1 ^ (hash2 << 1);           // 移位+异或组合，降低碰撞概率
//     }
// };
// }

// enum class PlayState {
//     Stopped,
//     Playing,
//     Paused
// };

// struct MediaSourceInfo {
//     LocalVideoSource* videoSource = nullptr;       // 视频源
//     MediaAudioSource* audioSource = nullptr;       // 音频源
//     MediaSourceTaskThread* mediaThread = nullptr;  // 对应的处理线程
//     MediaClock* mediaClock = nullptr;             // 同步时钟
//     PlayState state = PlayState::Playing;          // 播放状态
//     double totalDuration = 0.0;                    // 总时长
//     double currentTime = 0.0;                      // 当前播放时间
//     double speed = 1.0;
//     AudioItemWidget* audioWidget = nullptr;        // 关联的音频UI组件
//     int sceneId  = -1;
// };

// class ThreadPool;
// class MediaSourceController : public QObject {
//     Q_OBJECT
// public:

//     explicit MediaSourceController(ThreadPool* threadPool,QObject* parent = nullptr);
//     ~MediaSourceController() override;

//     bool addMediaSource(LocalVideoSource* videoSource, MediaAudioSource* audioSource,AudioItemWidget* audioWidget = nullptr);
//     void removeMediaSource(int sourceId = -1);
//     // 播放控制接口
//     void play(int sourceId,int sceneId = -1);
//     void pause(int sourceId,int sceneId = -1);
//     void stop(int sourceId,int sceneId = -1);
//     bool seek(int sourceId, double sec);
//     void setSpeed(int sourceId, MediaSpeedFilter::Speed speed);

//     // 状态获取
//     double duration(int sourceId) const;
//     double currentTime(int sourceId) const;
//     bool isPlaying(int sourceId) const;
//     bool isPaused(int sourceId) const;
//     std::vector<std::shared_ptr<FrameQueue>> getMediaFrameQueue() const;
//     void setRenderWidget(CudaRenderWidget* widget) { cudaRenderWidget_ = widget; }
//     void setSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager;}
//     void setRecording(bool record,int sourceId = -1);
//     void stopAllMediaSources();
// signals:
//     void playStateChanged(int sourceId,bool isPlaying);
//     void timeChanged(int sourceId,int64_t currentUs);
//     void mediaEnded(int sourceId);

// private:
//     double calculateCurrentTime(int sourceId);
//     void resetMediaSource(MediaSourceInfo& sourceInfo, int sourceId,int sceneId = -1);
//     MasterClockType matchMasterClockType(LocalVideoSource* source,MediaAudioSource* audioSource);
//     int resolveTargetSceneId(int sceneId) const;
//     bool isMediaInTargetScene(const MediaSourceInfo& sourceInfo, int targetSceneId) const;

// private:

//     ThreadPool* threadPool_;
//     MediaClock* mediaClock_;

//     CudaRenderWidget* cudaRenderWidget_ = nullptr;
//     SceneManager* sceneManager_ = nullptr;
//     std::unordered_map<int, MediaSourceInfo> mediaSources_;
//     mutable std::mutex sourcesMutex_;  // 保护多源容器的线程安全锁


// };

// #endif // MEDIASOURCECONTROLLER_H

#ifndef MEDIASOURCECONTROLLER_H
#define MEDIASOURCECONTROLLER_H

#include <QObject>
#include <memory>
#include <mutex>
#include "source/video/localvideosource.h"
#include "source/audio/mediaaudiosource.h"
#include "thread/mediasourcetaskthread.h"
#include "component/audioitemwidget.h"
#include "sync/mediaclock.h"
#include "thread/threadpool.h"

using MediaSourceKey = TaskKey;



enum class PlayState {
    Stopped,
    Playing,
    Paused
};

struct MediaSourceInfo {
    LocalVideoSource* videoSource = nullptr;       // 视频源
    MediaAudioSource* audioSource = nullptr;       // 音频源
    MediaSourceTaskThread* mediaThread = nullptr;  // 对应的处理线程
    MediaClock* mediaClock = nullptr;             // 同步时钟
    PlayState state = PlayState::Playing;          // 播放状态
    double totalDuration = 0.0;                    // 总时长
    double currentTime = 0.0;                      // 当前播放时间
    double speed = 1.0;
    AudioItemWidget* audioWidget = nullptr;        // 关联的音频UI组件
    int sceneId  = -1;
};

class ThreadPool;
class MediaSourceController : public QObject {
    Q_OBJECT
public:

    explicit MediaSourceController(ThreadPool* threadPool,QObject* parent = nullptr);
    ~MediaSourceController() override;

    bool addMediaSource(LocalVideoSource* videoSource, MediaAudioSource* audioSource,AudioItemWidget* audioWidget = nullptr);
    void removeMediaSource(int sourceId = -1, int sceneId = -1);
    // 播放控制接口
    void play(int sourceId,int sceneId = -1);
    void pause(int sourceId,int sceneId = -1);
    void stop(int sourceId,int sceneId = -1);
    bool seek(int sourceId, double sec,int sceneId = -1);
    void setSpeed(int sourceId, MediaSpeedFilter::Speed speed,int sceneId = -1);

    // 状态获取
    double duration(int sourceId,int sceneId = -1) const;
    double currentTime(int sourceId,int sceneId = -1) const;
    bool isPlaying(int sourceId,int sceneId = -1) const;
    bool isPaused(int sourceId,int sceneId = -1) const;
    std::vector<std::shared_ptr<FrameQueue>> getMediaFrameQueue() const;
    void setRenderWidget(CudaRenderWidget* widget) { cudaRenderWidget_ = widget; }
    void setSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager;}
    void setRecording(bool record,int sourceId = -1,int sceneId = -1);
    void stopAllMediaSources();
signals:
    void playStateChanged(int sourceId,bool isPlaying,int sceneId);
    void timeChanged(int sourceId,int64_t currentUs,int sceneId);
    void mediaEnded(int sourceId,int sceneId);

private:
    double calculateCurrentTime(int sourceId,int sceneId = -1);
    void resetMediaSource(MediaSourceInfo& sourceInfo, int sourceId,int sceneId = -1);
    MasterClockType matchMasterClockType(LocalVideoSource* source,MediaAudioSource* audioSource);
    int resolveTargetSceneId(int sceneId) const;
    bool isMediaInTargetScene(const MediaSourceInfo& sourceInfo, int targetSceneId) const;

private:

    ThreadPool* threadPool_;
    MediaClock* mediaClock_;

    CudaRenderWidget* cudaRenderWidget_ = nullptr;
    SceneManager* sceneManager_ = nullptr;
    std::unordered_map<MediaSourceKey, MediaSourceInfo> mediaSources_;
    mutable std::mutex sourcesMutex_;  // 保护多源容器的线程安全锁


};

#endif // MEDIASOURCECONTROLLER_H

