#include "streamcontroller.h"
#include "sync/globalclock.h"
#include "component/statisticsdialog.h"
#include <QOffscreenSurface>
#include <QDateTime>
// StreamController::StreamController()
// {


// }

StreamController::StreamController(ThreadPool* threadPools,CudaRenderWidget* openglWidget,NetworkMonitor* networkMonitor,QObject* parent)
    :QObject(parent),networkMonitor_(networkMonitor),threadPools_(threadPools),openglWidget_(openglWidget)
{

    fpsCounter_ = std::make_unique<FPSCounter>();
    syncClock_ = std::make_unique<AVSyncClock>();
    vPktQueue_ = std::make_unique<PacketQueue>();
    aPktQueue_ = std::make_unique<PacketQueue>();
    muxerManager_ = std::make_unique<MuxerManager>();
    vEncoder_ = std::make_unique<VEncoder>(vPktQueue_.get()); // 传递原始指针
    aEncoder_ = std::make_unique<AEncoder>(aPktQueue_.get());
    mixProcessor_ = std::make_unique<AudioMixProcessor>(syncClock_.get());
    if(networkMonitor_) networkMonitor_->setVEncoder(vEncoder_.get());
    dynamicBitrateCtrl_ = std::make_unique<DynamicBitrateController>(vPktQueue_.get(), aPktQueue_.get(),vEncoder_.get(),aEncoder_.get(),networkMonitor_);
    openglWidget_->setSyncClock(syncClock_.get());

    StatusBarManager::getInstance().bindFPSCounter(fpsCounter_.get());
    StatisticsDialog::getInstance()->setNetWorkMonitors(networkMonitor_);
    StatisticsDialog::getInstance()->setEncodeConfig(vEncoder_.get(),fpsCounter_.get());
    connect(dynamicBitrateCtrl_.get(), &DynamicBitrateController::bitrateAdjusted,
            this, [](int oldBitrate, int newBitrate, BitrateAdjustState state) {
                QString stateStr = (state == BitrateAdjustState::Raising) ? "回升" : "降低";
                qDebug() << "[UI通知] 码率" << stateStr << "："
                         << oldBitrate/1000 << "Kbps → " << newBitrate/1000 << "Kbps";
            });

}

StreamController::~StreamController()
{
    stop();
}

void StreamController::init(StreamConfig config)
{
    config_ = config;
    fpsCounter_->setMaxFPS(config.vEnConfig.framerate);
    dynamicBitrateCtrl_->init(config.vEnConfig.bitrate,config.aEnConfig.bitRate,40);
}


bool StreamController::start()
{
    stop();
    syncClock_->reset();

    // 创建 muxer
    if (config_.enableRecord) {
        if(!muxerManager_->addOutput(MuxerType::Record,config_.filePath,config_.vEnConfig.format)) return false;
    }

    if (config_.enableStream) {
        if(!muxerManager_->addOutput(MuxerType::Push,config_.streamUrl,"flv")) return false;
    }

    // 初始化编码器
    if(!aEncoder_->init(config_.aEnConfig)) return false;
    if(!vEncoder_->init(config_.vEnConfig)) return false;


    // 初始化muxer
    if(!muxerManager_->addStream(aEncoder_->getCodecContext(),AVMEDIA_TYPE_AUDIO))
    {
        qDebug() << "Failed to add audio stream to muxer";
        return false;
    }

    if(!muxerManager_->addStream(vEncoder_->getCodecContext(),AVMEDIA_TYPE_VIDEO))
    {
        qDebug() << "Failed to add video stream to muxer";
        return false;
    }

    // 获取音频帧队列
    std::vector<ASourceTaskThread*> audioSources = threadPools_->getATasks();
    std::unordered_map<int,MediaSourceTaskThread*> mediaSources = threadPools_->getMediaTasks();

    // 系统音频
    for (auto src : audioSources) {
        if (src) aFrameQueues_.push_back(src->frameQueue());
    }
    // 媒体音频
    for(auto src : mediaSources)
    {
        if(src.second) aFrameQueues_.push_back(src.second->aFrameQueue());
    }

    // TODO 初始化激活的音频流，创建setter
    // 初始化混音器
    if(!mixProcessor_->init(aFrameQueues_))
    {
        qDebug() << "mixProcessor_ init failed!";
        return false;
    }

    // 写入muxer头部
    if (!muxerManager_->startAll()) {
        qDebug() << "Failed to write header for muxer";
        return false;
    }

    // 初始化时钟
    syncClock_->initAudio(config_.aEnConfig.sampleRate,1024);
    syncClock_->initVideo(config_.vEnConfig.framerate);
    syncClock_->start(ClkMode::Realtime);

    // 开启网络监测和动态码率监测
    if (config_.enableStream) {
        dynamicBitrateCtrl_->start();    // 启动动态码率控制
    }


    isRunning_ = true;

    // 开启编码和复用线程
    videoThread_ = std::thread(&StreamController::videoEncodeLoop, this);
    audioThread_ = std::thread(&StreamController::audioEncodeLoop, this);
    videoMuxThread_ = std::thread(&StreamController::videoMuxLoop, this);
    audioMuxThread_ = std::thread(&StreamController::audioMuxLoop, this);

    return true;
}

void StreamController::stop()
{
    if (!isRunning_) return;

    disconnect(openglWidget_, &CudaRenderWidget::frameRecorded,this, &StreamController::onNewFrameAvailable);

    vEncoder_->flush();
    isRunning_ = false;

    encodeCond_.notify_all();
    if (dynamicBitrateCtrl_) dynamicBitrateCtrl_->stop();
    if (videoThread_.joinable()) videoThread_.join();
    if (videoMuxThread_.joinable()) videoMuxThread_.join();
    if (audioThread_.joinable()) audioThread_.join();
    if (audioMuxThread_.joinable()) audioMuxThread_.join();


    clearFramePtsQueue();
    vEncoder_->close();
    aEncoder_->close();
    mixProcessor_->stop();
    qDebug() << "视频队列剩余包数:" << vPktQueue_->size();
    qDebug() << "音频队列剩余包数:" << aPktQueue_->size();
    muxerManager_->stopAll();
    aFrameQueues_.clear();

}


// void StreamController::stopPushing()
// {

//     std::lock_guard<std::mutex> locker(pushMutex_);
//     if (isPushingStopped_ || !isRunning_) {
//         qDebug() << "[stopPushing] 推流已停止或未运行，无需重复操作";
//         return;
//     }

//     qDebug() << "[stopPushing] 开始停止推流（服务器断开触发）";
//     isPushingStopped_ = true;

//     if (muxerManager_ && config_.enableStream) {
//         muxerManager_->stopAll();
//         qDebug() << "[stopPushing] 已断开与服务器的推流连接";
//     }

//     if (vPktQueue_) {
//         vPktQueue_->clear();
//         qDebug() << "[stopPushing] 视频编码队列已清空";
//     }
//     if (aPktQueue_) {
//         aPktQueue_->clear();
//         qDebug() << "[stopPushing] 音频编码队列已清空";
//     }

//     threadPools_->stopAddAudioFrame();
//     pause();

//     qDebug() << "[stopPushing] 推流停止完成";
// }

// void StreamController::resumePushing()
// {

//     std::lock_guard<std::mutex> locker(pushMutex_);
//     if (!isPushingStopped_ || !isRunning_) {
//         qDebug() << "[resumePushing] 推流未停止或未运行，无需恢复";
//         return;
//     }

//     qDebug() << "[resumePushing] 开始恢复推流（TCP重连成功触发）";

//     isPushingStopped_ = false;

//     // 重启muxer
//     if (muxerManager_ && config_.enableStream) {
//         muxerManager_->removeOutput(MuxerType::Push);
//         if (!muxerManager_->addOutput(MuxerType::Push, config_.streamUrl, "flv")) {
//             qWarning() << "[resumePushing] 重新添加推流输出失败";
//             return;
//         }

//         if (!muxerManager_->addStream(aEncoder_->getCodecContext(), AVMEDIA_TYPE_AUDIO) ||
//             !muxerManager_->addStream(vEncoder_->getCodecContext(), AVMEDIA_TYPE_VIDEO)) {
//             qWarning() << "[resumePushing] 重新添加流失败";
//             return;
//         }
//         // 启动muxer（写入推流头部，建立正式连接）
//         if (!muxerManager_->startAll()) {
//             qWarning() << "[resumePushing] 重启复用器失败";
//             return;
//         }
//         qDebug() << "[resumePushing] 已重建与服务器的推流连接";
//     }

//     threadPools_->startAddAudioFrame();
//     resume();

//     encodeCond_.notify_all();

//     qDebug() << "[resumePushing] 推流恢复完成";
// }
void StreamController::pause()
{
    std::lock_guard<std::mutex> locker(pauseMutex_);
    if(!isPaused_ && isRunning_)
    {
        isPaused_.store(true);
        // mixProcessor_->pauseCleanup();
        syncClock_->pause();

        qDebug() << "录制已暂停";
    }
}

void StreamController::resume()
{
    std::lock_guard<std::mutex> locker(pauseMutex_);
    if(isPaused_ && isRunning_)
    {
        isPaused_.store(false);
        syncClock_->resume();
        // openglWidget_->setRecording(true);
        qDebug() << "录制已恢复";
    }
}


void StreamController::videoEncodeLoop() {
    // 1. 获取主线程的 OpenGL 上下文和 CUDA 互操作工具
    CudaInteropHelper* cudaHelper = openglWidget_->getEncoderCudaHelper();
    QOpenGLContext* mainGLContext = openglWidget_->getMainGLContext();
    if (!cudaHelper || !mainGLContext) {
        qDebug() << "缺少主线程 OpenGL 上下文或 CUDA 工具";
        return;
    }


    // 2. 子线程创建共享的 OpenGL 上下文
    std::unique_ptr<QOpenGLContext> threadGLContext(new QOpenGLContext());

    if (threadGLContext->isValid()) {
        threadGLContext->doneCurrent(); // 【新增】确保旧上下文释放
    }

    threadGLContext->setShareContext(mainGLContext);
    if (!threadGLContext->create()) {
        qDebug() << "子线程创建 OpenGL 共享上下文失败";
        return;
    }

    // 3. 子线程绑定与主线程相同的 CUDA 设备
    cudaError_t err = cudaSetDevice(cudaHelper->getDeviceId());
    if (err != cudaSuccess) {
        qDebug() << "子线程设置 CUDA 设备失败:" << cudaGetErrorString(err);
        return;
    }

    std::unique_ptr<QOffscreenSurface> offscreenSurface(new QOffscreenSurface());
    offscreenSurface->setFormat(threadGLContext->format());
    offscreenSurface->create();
    if (!offscreenSurface->isValid()) {
        qWarning() << "OffscreenSurface 无效";
        return;
    }

    int64_t lastVideoPts = -1;
    int videoFrameCount = 0;

    while (isRunning_) {
        // 暂停
        while (isPaused_ && isRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!isRunning_) break;

        CudaInteropHelper* cudaHelper = openglWidget_->getEncoderCudaHelper();
        std::unique_lock<std::mutex> lock(encodeMutex_);
        encodeCond_.wait(lock, [this]() { return !cudaFramePtsQueue_.empty() || !isRunning_; });
        if (!isRunning_) {
            // 停止时清空队列，不处理残留帧
            while (!cudaFramePtsQueue_.empty()) {
                cudaFramePtsQueue_.pop();
            }
            break;
        }
         // 积累的帧数
        int64_t framePts = cudaFramePtsQueue_.front();
        cudaFramePtsQueue_.pop();
        lock.unlock();

        if (!threadGLContext->makeCurrent(offscreenSurface.get())) {
            qDebug() << "子线程激活 OpenGL 上下文失败";
            threadGLContext->doneCurrent();
            continue;
        }


        cudaArray_t fboArray = cudaHelper->mapFboCudaArray();
        if(!fboArray)
        {
            qDebug() << "mapFboCudaArray is null";
            threadGLContext->doneCurrent();
            continue;
        }

        videoFrameCount++;
        if (lastVideoPts != -1) {
            int64_t ptsInterval = framePts - lastVideoPts; // 计算间隔（微秒）
            int targetFps = config_.vEnConfig.framerate;
            int64_t targetInterval = 1000000 / targetFps; // 目标间隔（如30fps→33333）
            int64_t diff = abs(ptsInterval - targetInterval);

            // 输出PTS日志（关键验证信息）
            // qDebug() << "[视频PTS] 帧序号:" << videoFrameCount
            //          << "PTS:" << framePts << "微秒"
            //          << "间隔:" << ptsInterval << "微秒"
            //          << "(目标:" << targetInterval << "，偏差:" << diff << ")";

            // 间隔异常报警（偏差超过10ms）
            if (diff > 10000) {
                qWarning() << "[视频PTS异常] 间隔偏差过大！可能导致播放速度异常";
            }
            // PTS回退报警
            if (framePts <= lastVideoPts) {
                qWarning() << "[视频PTS异常] PTS回退！上一帧:" << lastVideoPts << "当前帧:" << framePts;
            }
        }
        lastVideoPts = framePts;
        vEncoder_->setNextPts(framePts);

        static int encodeCount = 0;
        static auto lastTime = std::chrono::high_resolution_clock::now();
        auto startTime = std::chrono::steady_clock::now();
        // 3. 编码成功后，根据实际间隔更新时钟（扣除编码耗时，避免间隔被拉长）
        if (vEncoder_->encode(fboArray)) {


            encodeCount++;
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastTime).count() >= 1) {
                qDebug() << "编码次数/秒:" << encodeCount;
                encodeCount = 0;
                lastTime = now;
            }
            fpsCounter_->tick();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        qDebug() << "单帧编码时长" << duration.count() << "ms";
        cudaHelper->unmapFboCudaArray();
        threadGLContext->doneCurrent();

    }
}

void StreamController::onNewFrameAvailable(int64_t framePts)
{

    if (isPaused_) {
        return;
    }
    static int64_t last = 0;
    int64_t now = av_gettime(); // 修正：使用av_gettime()替代av_gettime_us()
    if (last != 0) {
        int64_t interval = now - last;
        if (interval > 50000) { // 超过50ms的间隔需重点关注
            qWarning() << "视频新帧间隔异常：" << interval << "us";
        }
    }
    last = now;
    {
        std::lock_guard<std::mutex> lock(encodeMutex_);
        cudaFramePtsQueue_.push(framePts);
    }
    encodeCond_.notify_one(); // 唤醒等待的编码子线程
}
void StreamController::clearFramePtsQueue() {
    std::lock_guard<std::mutex> lock(encodeMutex_);
    while (!cudaFramePtsQueue_.empty()) {
        cudaFramePtsQueue_.pop();
    }
    qDebug() << "[队列清空] 已清除残留PTS";
}

void StreamController::updateEncoderBitrate(int targetBitrate)
{

}

void StreamController::audioEncodeLoop() {

    if (!aEncoder_ || !muxerManager_ || !syncClock_) {
        qDebug() << "【audioEncodeLoop】编码器/混音器/时钟无效，退出";
        return;
    }

    int64_t lastAudioPts = -1;
    int audioFrameCount = 0;
    int64_t targetAudioInterval = (1024 * 1000000LL) / config_.aEnConfig.sampleRate; // 目标间隔（如1024/48000→21333微秒）

    while (isRunning_) {
        // 暂停
        while (isPaused_ && isRunning_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!isRunning_) break;

        AVFrame* mixedFrame = mixProcessor_->getMixedFrame();
        if (!mixedFrame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            qDebug() << "wait new frame";
            continue;
        }

        int64_t audioPts = mixedFrame->pts;

        audioFrameCount++;
        if (lastAudioPts != -1) {
            int64_t ptsInterval = audioPts - lastAudioPts;
            int64_t diff = abs(ptsInterval - targetAudioInterval);

            // qDebug() << "[音频PTS] 帧序号:" << audioFrameCount
            //          << "PTS:" << audioPts << "微秒"
            //          << "间隔:" << ptsInterval << "微秒"
            //          << "(目标:" << targetAudioInterval << "，偏差:" << diff << ")";

            if (diff > 10000) { // 偏差超过10ms报警
                qWarning() << "[音频PTS异常] 间隔偏差过大！" << diff;
            }
            if (audioPts <= lastAudioPts) { // 回退报警
                qWarning() << "[音频PTS异常] PTS回退！上一帧:" << lastAudioPts << "当前帧:" << audioPts;
            }
        }
        lastAudioPts = audioPts;


        bool encodeOk = aEncoder_->encode(mixedFrame);
        // 4. 编码成功后，更新时钟（用实际处理间隔）
        if (encodeOk) {
            lastAudioFramePts_ = syncClock_->getLastAPts();
            GlobalPool::getFramePool().recycle(mixedFrame);
        }
    }

    flushAudioMixer();
    aEncoder_->flush(muxerManager_.get());
}

void StreamController::flushAudioMixer() {
    if (!aEncoder_ || !mixProcessor_) {
        qWarning() << "冲刷音频时编码器或混音器无效";
        mixProcessor_->setFlushing(false);
        return;
    }

    QVector<std::shared_ptr<FrameQueue>> frameQueues = mixProcessor_->frameQueues();
    int maxQueueSize = 0;
    for (auto queue : frameQueues) {
        maxQueueSize = std::max(maxQueueSize, queue->size());
    }


    if(maxQueueSize == 0) {
        qDebug() << "无可冲刷的音频帧";
        mixProcessor_->setFlushing(false);
        return;
    }
    qDebug() << "初始队列剩余总帧数:" << maxQueueSize;
    syncClock_->setAPts(lastAudioFramePts_);
    mixProcessor_->setFlushing(true);

    qDebug() << "开始音频冲刷，最后正常帧PTS:" << lastAudioFramePts_;

    const int MAX_CONSECUTIVE_RETRIES = 50;
    const int MAX_FLUSH_FRAMES = maxQueueSize;
    int consecutiveRetries = 0;
    int totalFlushedFrames = 0;

    while (consecutiveRetries < MAX_CONSECUTIVE_RETRIES && totalFlushedFrames < MAX_FLUSH_FRAMES) {
        AVFrame* mixedFrame = mixProcessor_->getMixedFrame();
        if (!mixedFrame) {
            consecutiveRetries++;
            qDebug() << "连续无帧次数:" << consecutiveRetries;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }


        consecutiveRetries = 0;
        totalFlushedFrames++;

        bool encodeOk = aEncoder_->encodeFlushFrame(mixedFrame, muxerManager_.get());
        if (!encodeOk) {
            qWarning() << "冲刷帧编码失败（第" << totalFlushedFrames << "帧）";
        }

        GlobalPool::getFramePool().recycle(mixedFrame);
    }

    mixProcessor_->setFlushing(false);
    qDebug() << "音频冲刷完成，共处理" << totalFlushedFrames << "帧";
}


void StreamController::videoMuxLoop()
{

    int fps = config_.vEnConfig.framerate;
    int64_t sleepMs = static_cast<int64_t>(1000.0 / fps);
    while (isRunning_ || !vPktQueue_->isEmpty()) {

        if(vPktQueue_->isEmpty()){
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            continue;
        }

        AVPacket* pkt = vPktQueue_->pop();
        if (!pkt) {
            if (!isRunning_) break;
            qDebug() << "视频队列取出空包";
            continue;
        }

        muxerManager_->writePacket(pkt, AVMEDIA_TYPE_VIDEO);
        GlobalPool::getPacketPool().recycle(pkt);
    }
}

void StreamController::audioMuxLoop()
{
    while (isRunning_ || !aPktQueue_->isEmpty()) {
        // qDebug() << "audioMuxLoop aPktQueue_" << aPktQueue_;

        if(aPktQueue_->isEmpty()){
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        AVPacket* pkt = aPktQueue_->pop();
        if (!pkt) {
            if (!isRunning_) break;
            qDebug() << "音频队列取出空包";
            continue;
        }
        muxerManager_->writePacket(pkt, AVMEDIA_TYPE_AUDIO);
        GlobalPool::getPacketPool().recycle(pkt);
    }
}

