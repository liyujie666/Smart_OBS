#include "mediasourcecontroller.h"
#include "scene/scenemanager.h"
#include "scene/scene.h"
#include <QDebug>
#include <QLayout>


MediaSourceController::MediaSourceController(ThreadPool* threadPool,QObject *parent)
    :QObject(parent),threadPool_(threadPool)
{
    QTimer* timeTimer = new QTimer(this);
    connect(timeTimer, &QTimer::timeout, this, [this]() {
        std::lock_guard<std::mutex> lock(sourcesMutex_);
        for (auto& [key, sourceInfo] : mediaSources_) {
            if (sourceInfo.state == PlayState::Playing) {
                sourceInfo.currentTime = calculateCurrentTime(key.second,key.first);
                emit timeChanged(key.second, sourceInfo.currentTime,key.first);
            }
        }
    });
    timeTimer->start(1000);  // 1秒更新一次所有源的时间
}

MediaSourceController::~MediaSourceController()
{
    stopAllMediaSources();
}

bool MediaSourceController::addMediaSource(LocalVideoSource *videoSource, MediaAudioSource *audioSource,AudioItemWidget* audioWidget)
{
    if (!videoSource && !audioSource) {
        qCritical() << "媒体源未打开或fmtCtx为空";
        return false;
    }

    if (!sceneManager_) {
        qCritical() << "添加媒体失败：SceneManager未设置";
        return false;
    }

    int sourceId = videoSource ? videoSource->sourceId() : audioSource->sourceId();
    int sceneId = sceneManager_->currentScene()->id();
    std::lock_guard<std::mutex> lock(sourcesMutex_);

    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    if (mediaSources_.find(key) != mediaSources_.end()) {
        qWarning() << "场景" << sceneId << "的源" << sourceId << "已存在，无需重复添加";
        return false;
    }

    // 初始化单个源的信息
    MediaSourceInfo sourceInfo;
    sourceInfo.videoSource = videoSource;
    sourceInfo.audioSource = audioSource;
    sourceInfo.audioWidget = audioWidget;
    sourceInfo.state = PlayState::Stopped;
    sourceInfo.sceneId = sceneId;

    sourceInfo.mediaThread = new MediaSourceTaskThread(videoSource, audioSource, this);
    sourceInfo.mediaClock = new MediaClock();
    sourceInfo.mediaClock->setMasterClockType(matchMasterClockType(videoSource,audioSource));
    sourceInfo.mediaThread->setClock(sourceInfo.mediaClock);

    if (cudaRenderWidget_) {
        sourceInfo.mediaThread->setOpenGlWidget(cudaRenderWidget_);
    }

    threadPool_->addMediaTask(sceneId,sourceId,sourceInfo.mediaThread);

    connect(sceneManager_,&SceneManager::sceneIdUpdate,sourceInfo.mediaThread,&MediaSourceTaskThread::onSceneChanged);
    connect(sourceInfo.mediaThread, &MediaSourceTaskThread::playFinished, this, [this, sourceId,sceneId]() {
        std::lock_guard<std::mutex> lock(sourcesMutex_);
        qDebug() << "sourceId" << sourceId;
        MediaSourceKey key = std::make_pair(sceneId,sourceId);
        auto it = mediaSources_.find(key);
        if (it != mediaSources_.end()) {
            resetMediaSource(it->second,sourceId,sceneId);
            emit mediaEnded(sourceId,key.second);
        }
    });

    // 音频
    if (audioWidget) {
        // 音量
        connect(sourceInfo.mediaThread, &MediaSourceTaskThread::volumeLevelChanged,
                audioWidget, &AudioItemWidget::setVolumeLevel,
                Qt::QueuedConnection);
        // 增益调节
        connect(audioWidget, &AudioItemWidget::volumeGainChanged,
                sourceInfo.mediaThread, &MediaSourceTaskThread::setVolumeGain,
                Qt::QueuedConnection);
    }
    if(videoSource)
    {
        sourceInfo.totalDuration = videoSource->duration();
        // sourceInfo.speed = videoSource->fileSetting().speed / 10.0;
        // qDebug() << "speed" << sourceInfo.speed;
        // sourceInfo.mediaClock->setSpeed(sourceInfo.speed);
    }
    else if(audioSource)
        sourceInfo.totalDuration = audioSource->duration();

    mediaSources_[key] = sourceInfo;
    return true;
}

void MediaSourceController::removeMediaSource(int sourceId, int sceneId)
{
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    auto it = mediaSources_.begin();

    while (it != mediaSources_.end()) {
        // 解析当前媒体的场景ID和sourceId（从组合键中获取）
        int mediaSceneId = it->first.first;
        int mediaSourceId = it->first.second;
        auto& sourceInfo = it->second;

        // 过滤条件：只保留需要删除的媒体
        bool needDelete = false;
        // 模式1：删除指定“场景+源”
        if (sourceId != -1 && sceneId != -1) {
            needDelete = (mediaSceneId == sceneId && mediaSourceId == sourceId);
        }
        // 模式2：删除指定场景的所有源
        else if (sourceId == -1 && sceneId != -1) {
            needDelete = (mediaSceneId == sceneId);
        }
        // 模式3：删除所有源
        else if (sourceId == -1 && sceneId == -1) {
            needDelete = true;
        }

        if (needDelete) {
            // 1. 从 ThreadPool 中删除线程（传递媒体的场景ID和sourceId）
            threadPool_->removeMediaTask(mediaSourceId, mediaSceneId);
            // 2. 释放资源
            if (sourceInfo.mediaClock) {
                delete sourceInfo.mediaClock;
                sourceInfo.mediaClock = nullptr;
            }
            sourceInfo.mediaThread = nullptr; // ThreadPool 已删除线程
            // 3. 关闭媒体源
            if (sourceInfo.audioSource && !sourceInfo.videoSource) {
                sourceInfo.audioSource->close();
            } else if (sourceInfo.videoSource) {
                sourceInfo.videoSource->close();
            }
            // 4. 销毁音频控件
            if (sourceInfo.audioWidget) {
                if (QWidget* pw = sourceInfo.audioWidget->parentWidget()) {
                    if (QLayout* lay = pw->layout()) {
                        lay->removeWidget(sourceInfo.audioWidget);
                    }
                }
                sourceInfo.audioWidget->deleteLater();
                sourceInfo.audioWidget = nullptr;
            }

            qDebug() << "删除媒体：场景" << mediaSceneId << "，源" << mediaSourceId;
            it = mediaSources_.erase(it); // erase 后迭代器自动更新
        } else {
            ++it;
        }
    }
}

void MediaSourceController::play(int sourceId,int sceneId)
{
    std::lock_guard<std::mutex> lock(sourcesMutex_);

    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }

    MediaSourceKey key = std::make_pair(sceneId, sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) {
        qWarning() << "播放失败：场景" << sceneId << "的源" << sourceId << "不存在";
        return;
    }

    auto& sourceInfo = it->second;

    if (sourceInfo.state == PlayState::Playing) {
        return;
    }

    // 根据当前状态执行不同操作
    if (sourceInfo.state == PlayState::Stopped)
    {
        // AVFormatContext* fmtCtx = nullptr;
        if(sourceInfo.videoSource)
        {
            sourceInfo.mediaThread->setDemuxerFmtCtx(sourceInfo.videoSource->getFormatContext());
        }else
        {
            sourceInfo.mediaThread->setDemuxerFmtCtx(sourceInfo.audioSource->getFormatContext());
        }

        sourceInfo.mediaThread->init();
        sourceInfo.mediaThread->start();
    }
    else if (sourceInfo.state == PlayState::Paused) {
        sourceInfo.mediaThread->resume();
    }
    sourceInfo.state = PlayState::Playing;
    emit playStateChanged(sourceId, true,sceneId); // 信号携带源ID和状态
}

void MediaSourceController::pause(int sourceId,int sceneId)
{
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }

    MediaSourceKey key = std::make_pair(sceneId, sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) {
        qWarning() << "暂停失败：场景" << sceneId << "的源" << sourceId << "不存在";
        return;
    }

    auto& sourceInfo = it->second;
    if (sourceInfo.state != PlayState::Playing) return;

    sourceInfo.mediaThread->pause();
    sourceInfo.state = PlayState::Paused;
    emit playStateChanged(sourceId, false,sceneId);
}

void MediaSourceController::stop(int sourceId,int sceneId)
{
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId, sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) {
        qWarning() << "停止失败：场景" << sceneId << "的源" << sourceId << "不存在";
        return;
    }

    auto& sourceInfo = it->second;
    if (sourceInfo.state == PlayState::Stopped) return;

    resetMediaSource(sourceInfo, sourceId,sceneId);

    emit playStateChanged(sourceId, false,sceneId);
    emit mediaEnded(sourceId,sceneId);
}


void MediaSourceController::resetMediaSource(MediaSourceInfo& sourceInfo, int sourceId,int sceneId) {
    // 停止旧线程并销毁资源
    if(!sceneManager_) return;

    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }

    if (sourceInfo.sceneId != sceneId) {
        qWarning() << "重置媒体失败：场景不匹配（媒体所属" << sourceInfo.sceneId << "，目标" << sceneId << ")";
        return;
    }

    threadPool_->removeMediaTask(sourceId,sceneId);
    if(sourceInfo.mediaClock)
    {
        delete sourceInfo.mediaClock;
        sourceInfo.mediaClock = nullptr;
    }

    if(sourceInfo.mediaThread)
    {
        // delete sourceInfo.mediaThread;  //threadpool已经delete了
        sourceInfo.mediaThread = nullptr;
    }

    if (sourceInfo.videoSource) {
        sourceInfo.videoSource->seek(0);
    }
    if (sourceInfo.audioSource) {
        sourceInfo.audioSource->seek(0);
    }

    // 重建线程和时钟
    sourceInfo.mediaThread = new MediaSourceTaskThread(sourceInfo.videoSource, sourceInfo.audioSource, this);
    sourceInfo.mediaClock = new MediaClock();
    sourceInfo.mediaClock->setMasterClockType(matchMasterClockType(sourceInfo.videoSource,sourceInfo.audioSource));
    sourceInfo.mediaThread->setClock(sourceInfo.mediaClock);

    // 重新设置渲染窗口
    if (cudaRenderWidget_) {
        sourceInfo.mediaThread->setOpenGlWidget(cudaRenderWidget_);
    }

    threadPool_->addMediaTask(sceneId,sourceId,sourceInfo.mediaThread);

    // 重新连接playFinished信号
    connect(sourceInfo.mediaThread, &MediaSourceTaskThread::playFinished, this, [this, sourceId,sceneId]() {
        std::lock_guard<std::mutex> lock(sourcesMutex_);
        MediaSourceKey key = std::make_pair(sceneId, sourceId);
        auto it = mediaSources_.find(key);
        if (it != mediaSources_.end()) {
            resetMediaSource(it->second, sourceId, sceneId); // 递归调用，保持一致
            emit mediaEnded(sourceId,sceneId);
        }
    });

    // 新连接音频相关信号
    if (sourceInfo.audioWidget) {

        QObject::disconnect(sourceInfo.mediaThread, &MediaSourceTaskThread::volumeLevelChanged,
                            sourceInfo.audioWidget, &AudioItemWidget::setVolumeLevel);
        QObject::disconnect(sourceInfo.audioWidget, &AudioItemWidget::volumeGainChanged,
                            sourceInfo.mediaThread, &MediaSourceTaskThread::setVolumeGain);

        connect(sourceInfo.mediaThread, &MediaSourceTaskThread::volumeLevelChanged,
                sourceInfo.audioWidget, &AudioItemWidget::setVolumeLevel,
                Qt::QueuedConnection);
        connect(sourceInfo.audioWidget, &AudioItemWidget::volumeGainChanged,
                sourceInfo.mediaThread, &MediaSourceTaskThread::setVolumeGain,
                Qt::QueuedConnection);
    }

    // 重置状态和音频条
    sourceInfo.state = PlayState::Stopped;
    sourceInfo.currentTime = 0.0;
    if (sourceInfo.audioWidget) {
        sourceInfo.audioWidget->setVolumeLevel(-60.0);
    }
}

MasterClockType MediaSourceController::matchMasterClockType(LocalVideoSource *videoSource,MediaAudioSource* audioSource)
{
    if(videoSource && !audioSource)
        return MasterClockType::Video;
    else if (!videoSource && audioSource)
        return MasterClockType::Audio;
    else
        return MasterClockType::Audio;
}

int MediaSourceController::resolveTargetSceneId(int sceneId) const
{
    if (sceneId != -1) {
        return sceneId;
    }

    if (!sceneManager_) {
        qCritical() << "解析场景ID失败：sceneManager未设置，且未传入具体sceneId";
        return -2;
    }

    int currentSceneId = sceneManager_->currentScene()->id();
    return currentSceneId;
}

bool MediaSourceController::isMediaInTargetScene(const MediaSourceInfo &sourceInfo, int targetSceneId) const
{
    if (targetSceneId == -1) {
        return false;
    }

    bool isMatch = (sourceInfo.sceneId == targetSceneId);
    if (!isMatch) {
        qDebug() << "媒体场景不匹配：sourceId=" << sourceInfo.videoSource->sourceId()
            << "（所属场景=" << sourceInfo.sceneId << "，目标场景=" << targetSceneId << "）";
    }
    return isMatch;
}
bool MediaSourceController::seek(int sourceId, double sec,int sceneId) {

    std::lock_guard<std::mutex> lock(sourcesMutex_);

    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) {
        qWarning() << "seek失败：场景" << sceneId << "的源" << sourceId << "不存在";
        return false;
    }

    auto& sourceInfo = it->second;
    int seekUs = sec * 1000000;

    AVFormatContext* fmtCtx = nullptr;
    if (sourceInfo.videoSource) {
        fmtCtx = sourceInfo.videoSource->getFormatContext();
    } else if (sourceInfo.audioSource) {
        fmtCtx = sourceInfo.audioSource->getFormatContext();
    }
    if (!fmtCtx) {
        qCritical() << "源" << sourceId << "无有效上下文，seek失败";
        return false;
    }


    bool success = sourceInfo.mediaThread->seek(seekUs);
    if (success) {
        sourceInfo.currentTime = seekUs;
        emit timeChanged(sourceId, seekUs,sceneId);
    }
    return success;
}

void MediaSourceController::setSpeed(int sourceId, MediaSpeedFilter::Speed speed,int sceneId) {
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) {
        qWarning() << "setSpeed失败：场景" << sceneId << "的源" << sourceId << "不存在";
        return ;
    }

    it->second.mediaThread->setSpeed(speed);  // 调用线程的倍速设置方法

}

double MediaSourceController::calculateCurrentTime(int sourceId,int sceneId) {
    // std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    if (it == mediaSources_.end()) return 0.0;

    return it->second.mediaClock->getMaterClockPts();
}

double MediaSourceController::duration(int sourceId,int sceneId) const {
    // std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    return (it != mediaSources_.end()) ? it->second.totalDuration : 0.0;
}

double MediaSourceController::currentTime(int sourceId,int sceneId) const {
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    return (it != mediaSources_.end()) ? it->second.currentTime : 0.0;
}

bool MediaSourceController::isPlaying(int sourceId,int sceneId) const {
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    return (it != mediaSources_.end()) && (it->second.state == PlayState::Playing);
}

bool MediaSourceController::isPaused(int sourceId,int sceneId) const
{
    std::lock_guard<std::mutex> lock(sourcesMutex_);
    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }
    MediaSourceKey key = std::make_pair(sceneId,sourceId);
    auto it = mediaSources_.find(key);
    return (it != mediaSources_.end()) && (it->second.state == PlayState::Paused);
}

std::vector<std::shared_ptr<FrameQueue>> MediaSourceController::getMediaFrameQueue() const
{
    std::vector<std::shared_ptr<FrameQueue>> frameQueue;
    for(auto& source : mediaSources_)
    {
        frameQueue.push_back(source.second.mediaThread->aFrameQueue());
    }
    return frameQueue;
}

void MediaSourceController::setRecording(bool record, int sourceId, int sceneId)
{
    std::lock_guard<std::mutex> lock(sourcesMutex_); // 线程安全锁

    if(sceneId == -1){
        sceneId = sceneManager_->currentScene()->id();
    }

    for (auto& [key, sourceInfo] : mediaSources_) {
        // 键的结构：key.first=媒体所属场景ID，key.second=媒体源ID
        int mediaSceneId = key.first;
        int mediaSourceId = key.second;

        bool isSceneMatch = (mediaSceneId == sceneId);
        bool isSourceMatch = (sourceId == -1) || (mediaSourceId == sourceId);

        if (isSceneMatch && isSourceMatch) {
            // 4. 对符合条件的媒体设置录制状态
            if (sourceInfo.mediaThread) {
                sourceInfo.mediaThread->setRecording(record);
                qDebug() << "设置录制状态：场景" << mediaSceneId
                         << "，源" << mediaSourceId << "，状态=" << record;
            }
        }
    }
}


void MediaSourceController::stopAllMediaSources()
{
    removeMediaSource(-1, -1);
}
