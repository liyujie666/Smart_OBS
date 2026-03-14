#include "vsourcetaskthread.h"
#include "source/video/videosource.h"
#include "source/video/desktopsource.h"
#include "source/video/textsource.h"
#include "dxgi/cudacursorrenderer.h"
#include "sync/globalclock.h"
#include "transition/transitionmanager.h"
#include <QDebug>
#include <QPainter>
VSourceTaskThread::VSourceTaskThread(VideoSource *source,QObject* parent)
    : QObject(parent),videoSource_(source), currentSceneId_(source->sceneId())
{
    vDemuxer_ = std::make_unique<Demuxer>(videoSource_->getFormatContext());
    vDecoder_ = std::make_unique<VDecoder>(videoSource_);
    vPktQueue_ = std::make_shared<PacketQueue>();
    vFrameQueue_ = std::make_shared<FrameQueue>();

    isRunning_.store(false);
    if(source->type() == VideoSourceType::Text)
    {
        textSource_ = dynamic_cast<TextSource*>(source);

    }

}

VSourceTaskThread::~VSourceTaskThread()
{
    stop();
    releaseTextCudaResource();


}

void VSourceTaskThread::start()
{
    // DXGI线程
    if(videoSource_->type() == VideoSourceType::Desktop)
    {
        auto desktopSource = dynamic_cast<DesktopSource*>(videoSource_);
        if(desktopSource->screenSetting().captureType == DesktopCaptureType::DXGI)
        {
            dxgiDuplicator_ = desktopSource->dxgiDuplicator();
            if(!dxgiDuplicator_)
            {
                qDebug() << "dxgiDuplicator is null";
                return;
            }
            cudaRenderWidget_->setDXGIDuplicator(dxgiDuplicator_);
            connect(this, &VSourceTaskThread::frameReady, this, [=](const CudaFrameInfo& info) {
                cudaRenderWidget_->addOrUpdateLayer(info);
            }, Qt::QueuedConnection);


            isRunning_.store(true);
            vDxgiThread_ = std::thread(&VSourceTaskThread::runDxgiCapture,this);
        }
    }else if(videoSource_->type() == VideoSourceType::Text)
    {
        if (!cudaRenderWidget_ || !textSource_) {
            qCritical() << "TextSource init failed (nullptr)";
            return;
        }

        qRegisterMetaType<CudaFrameInfo>("CudaFrameInfo");
        connect(textSource_,&TextSource::textSettingChanged,this,&VSourceTaskThread::onTextSettingChanged,Qt::QueuedConnection);
        connect(videoSource_,&VideoSource::priorityUpdated,this, [this](int sourceId) {
            generateTextFrame(textSource_->textSetting(), textSource_->frameSize());
        });

        connect(this, &VSourceTaskThread::frameReady, this, [=](const CudaFrameInfo& info) {
            cudaRenderWidget_->addOrUpdateLayer(info);
        }, Qt::QueuedConnection);

        // 初始化第一次文本帧
        isRunning_.store(true);
        generateTextFrame(textSource_->textSetting(), textSource_->frameSize());

    }else
    {
        // ffmpeg线程
        vDemuxer_->init(nullptr,vPktQueue_.get());
        AVStream* vStream = vDemuxer_->getVStream();
        vDecoder_->init(vStream,vFrameQueue_.get());


        qRegisterMetaType<CudaFrameInfo>("CudaFrameInfo");
        connect(vDecoder_.get(), &VDecoder::frameReady, this, [=](const CudaFrameInfo& info) {
            // if(currentSceneId_ == videoSource_->sceneId() && !TransitionManager::getInstance()->isTransitioning()){
            //     cudaRenderWidget_->addOrUpdateLayer(info);
            // }
            cudaRenderWidget_->addOrUpdateLayer(info);
        }, Qt::QueuedConnection);

        isRunning_.store(true);
        vDemuxThread_ = std::thread(&VSourceTaskThread::runDemux,this);
        vDecodeThread_ = std::thread(&VSourceTaskThread::runDecode,this);
    }

}

void VSourceTaskThread::stop()
{
    if(!isRunning_.load()) return;
    isRunning_.store(false);

    disconnect(this, &VSourceTaskThread::frameReady, this, nullptr);
    if (videoSource_->type() ==  VideoSourceType::Text) {
        disconnect(textSource_, &TextSource::textSettingChanged,this, &VSourceTaskThread::onTextSettingChanged);
    }
    if(vDxgiThread_.joinable()) vDxgiThread_.join();
    if(vDecodeThread_.joinable()) vDecodeThread_.join();
    if(vDemuxThread_.joinable()) vDemuxThread_.join();

    vDecoder_->stop();
    vDemuxer_->stop();

    vFrameQueue_->close();
    vPktQueue_->close();
    vFrameQueue_->clear();
    vPktQueue_->clear();

    vDecoder_->close();
    vDemuxer_->close();
    videoSource_->close();

    if(cudaRenderWidget_)
    {
        cudaRenderWidget_->removeLayerBySourceId(videoSource_->sourceId());
    }

    if (videoSource_->type() == VideoSourceType::Text) {
        releaseTextCudaResource();
    }
    qDebug() << "VSourceTaskThread stopped!";
}


bool VSourceTaskThread::isRunning()
{
    return isRunning_.load();
}


FrameQueue *VSourceTaskThread::frameQueue() const
{
    return vFrameQueue_.get();
}

void VSourceTaskThread::setOpenGLWidget(CudaRenderWidget* widget)
{
    cudaRenderWidget_ = widget;
}

void VSourceTaskThread::setFPS(int fps)
{
    fps_ = fps;
    updateInterval_ = 1000000.0 / fps;
}

int VSourceTaskThread::videoSourceId() const
{
    if(!videoSource_) return -1;
    return videoSource_->sourceId();
}

void VSourceTaskThread::onTextSettingChanged(const TextSetting &newSetting, const QSize &frameSize)
{
    if (!isRunning_.load()) return;
    generateTextFrame(newSetting, frameSize);
}

void VSourceTaskThread::onSceneChanged(int sceneId)
{
    currentSceneId_ = sceneId;
}

void VSourceTaskThread::runDemux()
{
    while(isRunning_)
    {
        int ret = vDemuxer_->demux();
        if(ret < 0)
        {
            break;
        }
    }
}

void VSourceTaskThread::runDecode()
{
    while(isRunning_)
    {
        AVPacket* pkt = vPktQueue_->pop();
        if(!pkt) continue;

        vDecoder_->decode(pkt);
        GlobalPool::getPacketPool().recycle(pkt);
    }
}

void VSourceTaskThread::runDxgiCapture()
{
    DesktopSource* desktopSource = dynamic_cast<DesktopSource*>(videoSource_);
    if (!dxgiDuplicator_ || !desktopSource) return;

    // 获取参数
    ScreenSetting setting = desktopSource->screenSetting();
    DXGI_MODE_ROTATION rotation = dxgiDuplicator_->rotation();
    RECT displayRect = dxgiDuplicator_->getDisplayRect();

    int width = dxgiDuplicator_->width();
    int height = dxgiDuplicator_->height();
    qDebug() << "width" << width << "height" << height;

    // 为鼠标数据创建固定内存
    uint8_t* d_desktopData = nullptr;
    cudaMalloc(&d_desktopData, width * height * 4);

    while (isRunning_)
    {

        // if(currentSceneId_ != videoSource_->sceneId() && !TransitionManager::getInstance()->isTransitioning())
        // {
        //     QThread::msleep(10);
        //     continue;
        // }
        // 获取一帧桌面纹理
        if (!dxgiDuplicator_->acquireFrame()) {
            QThread::msleep(10);
            continue;
        }
        // 获取 CUDA 映射数组
        cudaArray_t cuArray = dxgiDuplicator_->getCudaArray();
        if (!cuArray) {
            dxgiDuplicator_->unmapCudaArray();
            dxgiDuplicator_->releaseFrame();
            QThread::msleep(10);
            continue;
        }

        // 绘制鼠标
        if(setting.enaleDrawMouse)
        {
            CursorInfo cursorInfo;
            dxgiDuplicator_->getCursorInfo(cursorInfo);

            // qDebug() << "鼠标可见性:" << cursorInfo.visible;
            // qDebug() << "鼠标形状缓冲区是否为空:" << (cursorInfo.shapeInfo.buffer == nullptr);
            // qDebug() << "鼠标形状尺寸:" << cursorInfo.shapeInfo.dxgiShape.Width << "x" << cursorInfo.shapeInfo.dxgiShape.Height;
            // qDebug() << "鼠标位置:" << cursorInfo.position.x << "," << cursorInfo.position.y;

            // 调用CUDA渲染器绘制鼠标
            bool renderSuccess = cudaRenderCursor(cuArray, width, height, &cursorInfo, setting.enaleDrawMouse, displayRect,rotation);
            if (!renderSuccess) {
                // qDebug() << "CUDA绘制鼠标失败";
                // 检查CUDA错误
                cudaError_t err = cudaGetLastError();
                if (err != cudaSuccess) {
                    qDebug() << "CUDA错误:" << cudaGetErrorString(err);
                }
            }

            // 释放鼠标信息缓冲区
            if (cursorInfo.shapeInfo.buffer) {
                delete[] cursorInfo.shapeInfo.buffer;
                cursorInfo.shapeInfo.buffer = nullptr;
            }
        }
        // 构造渲染信息
        CudaFrameInfo info;
        info.timestamp = GlobalClock::getInstance().getCurrentUs();
        info.type = VideoSourceType::Desktop;
        info.format = FrameFormat::BGRA;
        info.bgraArray = cuArray;
        info.width = width;
        info.height = height;
        info.sourceId = videoSource_->sourceId();
        info.sceneId = videoSource_->sceneId();
        info.priority = videoSource_->priority();
        // info.frameIndex = ++cudaFrameCounts[videoSource_->sourceId()];
        emit frameReady(info);

        dxgiDuplicator_->unmapCudaArray();
        dxgiDuplicator_->releaseFrame();
        QThread::usleep(updateInterval_);
    }

    // 释放CUDA内存
    cudaFree(d_desktopData);
}

void VSourceTaskThread::generateTextFrame(const TextSetting &setting, const QSize &frameSize)
{

    qDebug() << "\n===== 绘制阶段接收的参数 =====";
    qDebug() << "文本内容: " << (setting.text.isEmpty() ? "空文本" : setting.text);
    qDebug() << "文本颜色(RGBA): " << QColor(setting.color).red() << ","
             << QColor(setting.color).green() << ","
             << QColor(setting.color).blue() << ","
             << QColor(setting.color).alpha() * setting.textOpacity;
    qDebug() << "背景颜色(RGBA): " << QColor(setting.bgColor).red() << ","
             << QColor(setting.bgColor).green() << ","
             << QColor(setting.bgColor).blue() << ","
             << QColor(setting.bgColor).alpha() * setting.bgOpacity;
    qDebug() << "文本框尺寸: " << frameSize.width() << "x" << frameSize.height();
    qDebug() << "=============================\n";


    // 1. 释放旧的CUDA缓存（避免内存泄漏）
    releaseTextCudaResource();

    // 2. QPainter绘制文本到QImage（仅绘制1次）
    QImage textImage(frameSize, QImage::Format_ARGB32);
    textImage.fill(Qt::transparent); // 背景透明（或用setting.bgColor）

    QPainter painter(&textImage);
    painter.setRenderHint(QPainter::Antialiasing); // 抗锯齿（文本更清晰）
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 2.1 应用文本配置
    painter.setFont(setting.font);
    QFont appliedFont = painter.font();
    QColor textColor(setting.color);
    textColor.setAlphaF(setting.textOpacity);
    textColor = QColor(textColor.blue(),textColor.green(),textColor.red(),textColor.alpha());
    painter.setPen(textColor);

    // 2.2 绘制背景（若需要）
    if (!setting.bgColor.isEmpty()) {
        QColor bgColor(setting.bgColor);
        bgColor.setAlphaF(setting.bgOpacity);
        bgColor = QColor(bgColor.blue(),bgColor.green(),bgColor.red(),bgColor.alpha());
        painter.fillRect(textImage.rect(), bgColor);
    }

    // 2.3 文本对齐+转换（大写/小写等）
    Qt::Alignment align = Qt::AlignLeft;
    switch (setting.horizontalAlign) {
        case HorizontalAlignment::Left: align |= Qt::AlignLeft; break;
        case HorizontalAlignment::Right: align |= Qt::AlignRight; break;
        case HorizontalAlignment::Center: align |= Qt::AlignHCenter; break;
        case HorizontalAlignment::Justify: align |= Qt::AlignJustify; break;
    }
    switch (setting.verticalAlign) {
        case VerticalAlignment::Top: align |= Qt::AlignTop; break;
        case VerticalAlignment::Middle: align |= Qt::AlignVCenter; break;
        case VerticalAlignment::Bottom: align |= Qt::AlignBottom; break;
    }

    // 2.4 文本转换处理
    QString drawText = setting.text.isEmpty() ? "默认文本" : setting.text;
    switch (setting.textTransform) {
        case TextTransform::UpperCase: drawText = drawText.toUpper(); break;
        case TextTransform::LowerCase: drawText = drawText.toLower(); break;
        case TextTransform::Capitalize:
            drawText = drawText.toLower();
            for (int i = 0; i < drawText.size(); ++i) {
                if (i == 0 || drawText.at(i-1).isSpace()) {
                    drawText[i] = drawText[i].toUpper();
                }
            }
            break;
        default: break;
    }

    painter.drawText(textImage.rect(),align | Qt::TextWordWrap | Qt::TextExpandTabs,drawText);
    painter.end();


    // 3. 转换QImage为CUDA格式（BGRA）
    textImage = textImage.rgbSwapped(); // ARGB → BGRA（适配CUDA/OpenGL）
    uchar* imageData = textImage.bits();
    int dataSize = textImage.bytesPerLine() * textImage.height();

    // 4. 分配CUDA数组并缓存（后续复用，直到配置变化）
    cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
    cudaError_t cudaErr = cudaMallocArray(&cachedBgraArray_, &channelDesc,
                                          frameSize.width(), frameSize.height());
    if (cudaErr != cudaSuccess) {
        qCritical() << "CUDA malloc failed:" << cudaGetErrorString(cudaErr);
        return;
    }
    // 复制图像数据到CUDA数组
    cudaMemcpyToArray(cachedBgraArray_, 0, 0, imageData, dataSize, cudaMemcpyHostToDevice);

    // 5. 构造帧信息并传给OpenGL渲染（仅1次，除非配置变化）
    // if(currentSceneId_ == videoSource_->sceneId() && !TransitionManager::getInstance()->isTransitioning())
    // {
        CudaFrameInfo textFrame;
        textFrame.timestamp = GlobalClock::getInstance().getCurrentUs();
        textFrame.type = VideoSourceType::Text;
        textFrame.format = FrameFormat::BGRA;
        textFrame.bgraArray = cachedBgraArray_; // 使用缓存的CUDA数组
        textFrame.width = frameSize.width();
        textFrame.height = frameSize.height();
        textFrame.sourceId = videoSource_->sourceId();
        textFrame.sceneId = videoSource_->sceneId();
        textFrame.priority = videoSource_->priority();
        emit frameReady(textFrame);
        qDebug() << "Text frame generated (sourceId:" << videoSource_->sourceId() << ")";

}

void VSourceTaskThread::releaseTextCudaResource()
{
    if (cachedBgraArray_) {
        cudaFreeArray(cachedBgraArray_);
        cachedBgraArray_ = nullptr;
    }
}

