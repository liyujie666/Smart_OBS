#ifndef SOURCETASKTHREAD_H
#define SOURCETASKTHREAD_H
#include "queue/framequeue.h"
#include "queue/packetqueue.h"
#include "demux/demuxer.h"
#include "decoder/vdecoder.h"
#include "dxgi/dxgidesktopduplicator.h"
#include "component/textsettingdialog.h"
#include "infotools.h"
#include <QObject>
#include <QThread>
extern"C"
{
#include "libavformat/avformat.h"
}
class VideoSource;
class AVSyncClock;
class TextSource;
class VSourceTaskThread :public QObject
{
    Q_OBJECT
public:
    VSourceTaskThread(VideoSource* source,QObject* parent=nullptr);
    ~VSourceTaskThread();

    void start();
    void stop();

    bool isRunning();
    FrameQueue* frameQueue() const;
    void setOpenGLWidget(CudaRenderWidget* widget);
    void setFPS(int fps = 30);
    int videoSourceId() const;
    void generateTextFrame(const TextSetting& setting, const QSize& frameSize);
    void releaseTextCudaResource();

signals:
    void frameReady(const CudaFrameInfo& info);
    // void setDxgiDuplicatorSignal(DxgiDesktopDuplicator* duplicator);

public slots:
    void onTextSettingChanged(const TextSetting& newSetting, const QSize& frameSize);
    void onSceneChanged(int sceneId);
private:
    void runDemux();
    void runDecode();
    void runDxgiCapture();




private:

    std::unique_ptr<Demuxer> vDemuxer_;
    std::unique_ptr<VDecoder> vDecoder_;
    std::shared_ptr<PacketQueue> vPktQueue_;
    std::shared_ptr<FrameQueue> vFrameQueue_;
    // Demuxer* vDemuxer_;
    // VDecoder* vDecoder_;
    // PacketQueue* vPktQueue_;
    // FrameQueue* vFrameQueue_;

    std::atomic<bool> isRunning_;
    std::thread vDemuxThread_;
    std::thread vDecodeThread_;
    std::thread vDxgiThread_;
    std::map<int,int64_t> cudaFrameCounts;
    CudaRenderWidget* cudaRenderWidget_;
    VideoSource* videoSource_;
    cudaGraphicsResource* dxgiCudaResource_ = nullptr;
    DxgiDesktopDuplicator* dxgiDuplicator_;
    int fps_;
    int64_t updateInterval_;
    int currentSceneId_ = 1;

    // 文本输入源相关
    TextSource* textSource_;
    cudaArray_t cachedBgraArray_{nullptr};
    QSize textFrameSize_;                   // 文本帧尺寸
    TextSetting lastTextSetting_;
};
#endif
