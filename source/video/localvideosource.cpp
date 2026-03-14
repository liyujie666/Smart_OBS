#include "source/video/localvideosource.h"
#include "component/filesettingdialog.h"
#include <QDebug>

LocalVideoSource::LocalVideoSource(int sourceId,int sceneId,FileSetting setting)
    : VideoSource(sourceId,sceneId),m_setting(setting)
{

}

LocalVideoSource::~LocalVideoSource()
{
    close();
}

int LocalVideoSource::open()
{
    if(localVideoFmtCtx_) {
        return -1;
    }

    if(avformat_open_input(&localVideoFmtCtx_, m_setting.filePath.toStdString().c_str(), nullptr, nullptr) < 0)
    {
        qDebug() << "File: avformat_open_input failed!";
        return -1;
    }

    int ret = avformat_find_stream_info(localVideoFmtCtx_,nullptr);
    if(ret < 0){
        avformat_close_input(&localVideoFmtCtx_);
        return ret;
    }

    for(size_t i = 0;i < localVideoFmtCtx_->nb_streams; ++i){
        AVStream* stream = localVideoFmtCtx_->streams[i];
        AVCodecParameters* codecPar = stream->codecpar;

        if(codecPar->codec_type == AVMEDIA_TYPE_AUDIO){
            audioIndex_ = i;
        }
        else if(codecPar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoIndex_ = i;
        }
    }

    return 0;
}

void LocalVideoSource::close()
{
    if (localVideoFmtCtx_) {
        avformat_close_input(&localVideoFmtCtx_);
        localVideoFmtCtx_ = nullptr;
        audioIndex_ = -1;
        videoIndex_ = -1;
    }
}

void LocalVideoSource::seek(int64_t us) {
    if (localVideoFmtCtx_) {
        av_seek_frame(localVideoFmtCtx_, -1, us, AVSEEK_FLAG_BACKWARD); // 跳转到0时刻
        avformat_flush(localVideoFmtCtx_); // 清空缓冲区
    }
}

MediaAudioSource *LocalVideoSource::getAssociatedAudioSource()
{
    return audioSource_;
}
AVFormatContext *LocalVideoSource::getFormatContext()
{
    return localVideoFmtCtx_;
}

QString LocalVideoSource::name() const { return m_setting.nickName; }
VideoSourceType LocalVideoSource::type() const { return VideoSourceType::Media; }

double LocalVideoSource::duration() const
{
    if(!localVideoFmtCtx_) return 0;
    return localVideoFmtCtx_->duration / (double)AV_TIME_BASE;
}

int LocalVideoSource::audioIndex() const
{
    return audioIndex_;
}

int LocalVideoSource::videoIndex() const
{
    return videoIndex_;
}

QString LocalVideoSource::filePath() const { return m_setting.filePath; }

void LocalVideoSource::serSetting(const FileSetting &setting)
{
    m_setting = setting;
}

FileSetting LocalVideoSource::fileSetting() const
{
    return m_setting;
}

void LocalVideoSource::setMediaASource(MediaAudioSource *audioSource)
{
    audioSource_ = audioSource;
}

void LocalVideoSource::setFormatContext(AVFormatContext *fmtCtx)
{
    localVideoFmtCtx_ = fmtCtx;
}

std::tuple<int, int, AVPixelFormat, AVRational> LocalVideoSource::getVideoFilterParams() const {
    if (!localVideoFmtCtx_) {
        return std::make_tuple(0, 0, AV_PIX_FMT_NONE, AVRational{0, 1});
    }

    std::tuple<int, int, AVPixelFormat, AVRational> videoParams =
        std::make_tuple(0, 0, AV_PIX_FMT_NONE, AVRational{0, 1});

    for (int i = 0; i < localVideoFmtCtx_->nb_streams; ++i) {
        AVStream* stream = localVideoFmtCtx_->streams[i];
        AVCodecParameters* codecPar = stream->codecpar;

        if (codecPar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoParams = std::make_tuple(
                codecPar->width,
                codecPar->height,
                static_cast<AVPixelFormat>(codecPar->format),
                stream->time_base
                );
            break;
        }
    }
    return videoParams;
}
