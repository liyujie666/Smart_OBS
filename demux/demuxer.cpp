#include "demux/demuxer.h"
#include <QDebug>
Demuxer::Demuxer(AVFormatContext* fmtCtx):fmtCtx_(fmtCtx)
{

}

Demuxer::Demuxer()
{

}

Demuxer::~Demuxer()
{
    close();
}

void Demuxer::init(const QString& url,const QString& format,PacketQueue* aPktQueue,PacketQueue* vPktQueue,int type)
{
    std::lock_guard<std::mutex> lock(demuxMutex_);
    m_url = url;
    m_format = format;
    aPktQueue_ = aPktQueue;
    vPktQueue_ = vPktQueue;
    type_ = type;
    isPaused_ = false;
    isStopped_ = false;
    initDemuxer();
}

void Demuxer::init(PacketQueue *aPktQueue, PacketQueue *vPktQueue)
{
    std::lock_guard<std::mutex> lock(demuxMutex_);

    aPktQueue_ = aPktQueue;
    vPktQueue_ = vPktQueue;
    isPaused_ = false;
    isStopped_ = false;
    initDemuxer();
}

int Demuxer::demux()
{
    std::lock_guard<std::mutex> lock(demuxMutex_);

    if (isSeeking_) {
        int64_t targetTs = seekTargetTs_; // 取出目标时间戳

        // 执行seek操作
        int ret = av_seek_frame(fmtCtx_, -1, targetTs, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            qDebug() << "seek失败：" << err << "，目标TS：" << targetTs;
        } else {
            qDebug() << "seek成功，目标TS：" << targetTs;

            if (aPktQueue_) aPktQueue_->clear();
            if (vPktQueue_) vPktQueue_->clear();
        }

        // 重置seek标志
        isSeeking_ = false;
    }


    while(isPaused_ && !isStopped_)
    {
        std::unique_lock<std::mutex> pauseLock(pauseMutex_);
        pauseCond_.wait(pauseLock,[this]{ return !isPaused_ || isStopped_; });
        pauseLock.unlock();
    }

    if (fmtCtx_ == nullptr || isStopped_) {
        return -1;
    }

    AVPacket* pkt = GlobalPool::getPacketPool().get();
    int ret = av_read_frame(fmtCtx_, pkt);
    if (ret < 0)
    {
        GlobalPool::getPacketPool().recycle(pkt);
        if (ret == AVERROR_EOF)
        {
            qDebug() << "av_read_frame : AVERROR_EOF";
            return 1;
        }
        else
        {
            printError(ret);
            return -1;
        }
    }

    if (aStream_ && pkt->stream_index == aStream_->index && aPktQueue_) {
        aPktQueue_->push(pkt);
    }
    else if (vStream_ && pkt->stream_index == vStream_->index && vPktQueue_) {
        vPktQueue_->push(pkt);

    }

    return 0;
}
AVStream *Demuxer::getAStream()
{
    return aStream_;
}

AVStream *Demuxer::getVStream()
{
    return vStream_;
}

void Demuxer::close()
{
    std::lock_guard<std::mutex> lock(demuxMutex_);
    isStopped_ = true;
    if (fmtCtx_) {
        fmtCtx_ = nullptr;
    }
    if(opts){
        av_dict_free(&opts);
    }

    qDebug() << "Demuxer closed!";

}

void Demuxer::stop()
{

    isStopped_ = true;
    pauseCond_.notify_all();
}

void Demuxer::pause()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    isPaused_ = true;
    pauseCond_.notify_all();
    qDebug() << "[demuxer] paused";
}

void Demuxer::resume()
{
    std::lock_guard<std::mutex> lock(pauseMutex_);
    isPaused_ = false;
    pauseCond_.notify_all();
    qDebug() << "[Demuxer] resumed";
}

void Demuxer::seek(int64_t targetUs)
{
    seekTargetTs_ = targetUs;
    isSeeking_ = true;
    qDebug() << "[Demuxer] seek start";
}


void Demuxer::initDemuxer()
{
    if(fmtCtx_ == nullptr) return;
    // int ret = avformat_find_stream_info(fmtCtx_,nullptr);
    // if(ret < 0){
    //     printError(ret);
    //     avformat_close_input(&fmtCtx_);
    //     return;
    // }

    for(size_t i = 0;i < fmtCtx_->nb_streams; ++i){
        AVStream* stream = fmtCtx_->streams[i];
        AVCodecParameters*codecPar = stream->codecpar;

        if(codecPar->codec_type == AVMEDIA_TYPE_AUDIO){
            aStream_ = stream;
            aTimeBase_ = stream->time_base;
        }
        else if(codecPar->codec_type == AVMEDIA_TYPE_VIDEO){
            vStream_ = stream;
            vTimeBase_ = stream->time_base;
        }
    }
}

void Demuxer::setFmtCtx(AVFormatContext *fmtCtx)
{
    fmtCtx_ = fmtCtx;
}

int Demuxer::getType()
{
    return type_;
}

void Demuxer::waitForSeekCompleted()
{
    std::unique_lock<std::mutex> lock(seekMutex_);
    seekCond_.wait(lock, [this] { return seekCompleted_; });
}


void Demuxer::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        qDebug() << "Unknow Error!";
    }
    else{
        qDebug() << "Error:" << errorBuffer;
    }
}
