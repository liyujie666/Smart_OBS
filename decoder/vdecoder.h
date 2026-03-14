#ifndef VDECODER_H
#define VDECODER_H
#include "render/cudarenderwidget.h"
#include "infotools.h"
#include "sync/seeksync.h"
#include "source/video/videosource.h"
extern "C"
{
#include<libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/cpu.h>
#include<libavutil/hwcontext.h>

}
#include <QObject>
#include<atomic>
#include<mutex>

class FrameQueue;
class VRescaler;
class MediaClock;
class MediaSpeedFilter;
struct VideoParams
{
    int width;
    int height;
    enum AVPixelFormat pixFmt;
    AVRational frameRate;
    AVRational timeBase;
};


class VDecoder : public QObject
{
    Q_OBJECT
public:
    explicit VDecoder(VideoSource* source);
    VDecoder(const VDecoder&) = delete;
    VDecoder& operator=(const VDecoder&) = delete;
    ~VDecoder();

    void init(AVStream *stream_,FrameQueue* frameQueue);
    void decode(AVPacket* packet);
    void flushDecoder();
    void setClock(MediaClock* clock);
    void setSeekSync(const std::shared_ptr<SeekSync>& s) { seekSync_ = s; }
    void setSpeedFilter(MediaSpeedFilter* speedFilter);
    int getDuration();
    VideoParams* getVideoPars();
    AVCodecContext *getCodecCtx();
    AVStream *getStream();
    void adjustVideoRender(int64_t diffUs, CudaFrameInfo* info);
    void stop();
    void pause();
    void resume();
    void flushQueue();
    void close();


signals:
    // void frameReady(cudaGraphicsResource_t * resource, int width, int height);
    void frameReady(const CudaFrameInfo& info);
private:
    void initRescaler();
    void initVideoParams(AVFrame* frame);
    void printError(int ret);


private:
    VideoSource* videoSource_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVStream* stream_ = nullptr;
    FrameQueue* frameQueue_ = nullptr;
    VideoParams* vInParams_ = nullptr;
    VideoParams* vOutParams_ = nullptr;
    VRescaler* rescale = nullptr;
    MediaClock* syncClock_ = nullptr;
    MediaSpeedFilter *speedFilter_ = nullptr;
    bool hwFlag = true;
    bool hasAudio_ = false;
    std::atomic<bool>m_stop;
    std::atomic<bool> m_paused = false;
    std::mutex m_mutex;
    std::condition_variable pauseCond_;
    std::mutex pauseMutex_;

    AVBufferRef* hwDeviceCtx_ = nullptr;
    enum AVHWDeviceType hwType_ = AV_HWDEVICE_TYPE_CUDA;
    enum AVPixelFormat hw_pix_fmt_;
    std::map<int,int64_t> cudaFrameCounts;
    bool isFirstFrame = true;
    std::shared_ptr<SeekSync> seekSync_;
    int localSerial_{0};
    bool firstFrameAfterSeekV = false;

};

#endif // VDECODER_H
