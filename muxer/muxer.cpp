#include "muxer.h"

#include "adapter_ffmpeg/ffmpeg_bridge.h"
#include "monitor/netmonitor.h"
#include "statusbar/statusbarmanager.h"
#include "ScopedTimer.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>

Muxer::Muxer() = default;

Muxer::~Muxer()
{
    close();
}

bool Muxer::init(const QString& url, MuxerType type, const QString& format)
{
    close();
    url_ = url;
    type_ = type;
    format_ = format;

    if (type_ == MuxerType::Push) {
        return initPublisher();
    }
    return initRecorder(format);
}

bool Muxer::initRecorder(const QString& format)
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString dirUrl = url_;
    if (!dirUrl.isEmpty() && !dirUrl.endsWith(QDir::separator())) {
        dirUrl += QDir::separator();
    }
    url_ = dirUrl + QString("%1.%2").arg(timestamp, format);

    const QByteArray encodedUrl = url_.toUtf8();
    int ret = avformat_alloc_output_context2(&fmtCtx_, nullptr, nullptr, encodedUrl.constData());
    if (ret < 0 || !fmtCtx_) {
        qWarning() << "Failed to allocate recording context for" << url_;
        return false;
    }

    if (!(fmtCtx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&fmtCtx_->pb, encodedUrl.constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            StatusBarManager::getInstance().showMessage(
                "文件路径 " + url_ + " 错误！", MessageType::Warning);
            avformat_free_context(fmtCtx_);
            fmtCtx_ = nullptr;
            return false;
        }
    }
    return true;
}

bool Muxer::initPublisher()
{
    if (!url_.startsWith("rtmp://", Qt::CaseInsensitive)) {
        qWarning() << "RTMP SDK only supports rtmp:// URLs:" << url_;
        return false;
    }

    rtmp::PublisherConfig config;
    config.url = url_.toStdString();
    config.connectTimeoutMs = 8000;
    config.statsIntervalMs = 1000;

    publisher_ = std::make_unique<rtmp::Publisher>(config);
    publisher_->onState([this](rtmp::SessionState state, const std::string& detail) {
        pushState_.store(state);
        const QString stateDetail = QString::fromStdString(detail);
        NetMonitor::instance()->updateSessionState(state, stateDetail);
        qInfo() << "[RTMP SDK]" << rtmp::toString(state) << stateDetail;
    });
    publisher_->onStats([](const rtmp::RtmpStats& stats) {
        NetMonitor::instance()->updateRtmpStats(stats);
    });
    return true;
}

bool Muxer::addStream(AVCodecContext* codecCtx, AVMediaType type)
{
    if (!codecCtx) return false;
    if (type_ == MuxerType::Push) {
        return configurePublisherStream(codecCtx, type);
    }
    return addRecordStream(codecCtx, type);
}

bool Muxer::addRecordStream(AVCodecContext* codecCtx, AVMediaType type)
{
    if (!fmtCtx_) return false;

    AVStream* stream = avformat_new_stream(fmtCtx_, nullptr);
    if (!stream || avcodec_parameters_from_context(stream->codecpar, codecCtx) < 0) {
        qWarning() << "Failed to add recording stream for" << url_;
        return false;
    }
    stream->time_base = codecCtx->time_base;

    if (type == AVMEDIA_TYPE_AUDIO) {
        audioStream_ = stream;
        timeBaseAudio_ = codecCtx->time_base;
    } else if (type == AVMEDIA_TYPE_VIDEO) {
        videoStream_ = stream;
        timeBaseVideo_ = codecCtx->time_base;
    } else {
        return false;
    }
    return true;
}

bool Muxer::configurePublisherStream(AVCodecContext* codecCtx, AVMediaType type)
{
    if (!publisher_) return false;

    if (type == AVMEDIA_TYPE_VIDEO) {
        if (codecCtx->codec_id != AV_CODEC_ID_H264) {
            qWarning() << "RTMP SDK requires H.264 video";
            return false;
        }
        rtmp::VideoParams params = rtmp::bridge::videoParamsFrom(codecCtx);
        if (params.sps.empty() || params.pps.empty()) {
            qWarning() << "Cannot extract H.264 SPS/PPS for RTMP publishing";
            return false;
        }
        publisher_->setVideoParams(params);
        timeBaseVideo_ = codecCtx->time_base;
        return true;
    }

    if (type == AVMEDIA_TYPE_AUDIO) {
        if (codecCtx->codec_id != AV_CODEC_ID_AAC) {
            qWarning() << "RTMP SDK requires AAC audio";
            return false;
        }
        rtmp::AudioParams params = rtmp::bridge::audioParamsFrom(codecCtx);
        publisher_->setAudioParams(params);
        timeBaseAudio_ = codecCtx->time_base;
        return true;
    }
    return false;
}

bool Muxer::writeHeader()
{
    if (headerWritten_) return true;

    if (type_ == MuxerType::Push) {
        if (!publisher_ || !publisher_->start()) {
            qWarning() << "Failed to start RTMP SDK publisher for" << url_;
            return false;
        }
        headerWritten_ = true;
        return true;
    }

    if (!fmtCtx_ || avformat_write_header(fmtCtx_, nullptr) < 0) {
        qWarning() << "Failed to write recording header for" << url_;
        return false;
    }
    headerWritten_ = true;
    return true;
}

bool Muxer::writePacket(AVPacket* pkt, AVMediaType type)
{
    if (!pkt || !headerWritten_) return false;
    return type_ == MuxerType::Push ? pushPacket(pkt, type)
                                    : writeRecordPacket(pkt, type);
}

bool Muxer::pushPacket(AVPacket* pkt, AVMediaType type)
{
    BENCHMARK_COND_SCOPE("rtmp_push_packet");
    if (!publisher_) return false;

    AVPacket packet = *pkt;
    std::vector<uint8_t> scratch;
    if (type == AVMEDIA_TYPE_VIDEO) {
        packet.time_base = timeBaseVideo_;
        rtmp::MediaSample sample = rtmp::bridge::fromVideoPacket(&packet, scratch);
        return publisher_->pushVideo(sample);
    }
    if (type == AVMEDIA_TYPE_AUDIO) {
        packet.time_base = timeBaseAudio_;
        rtmp::MediaSample sample = rtmp::bridge::fromAudioPacket(&packet, scratch);
        return publisher_->pushAudio(sample);
    }
    return false;
}

bool Muxer::writeRecordPacket(AVPacket* pkt, AVMediaType type)
{
    AVStream* stream = type == AVMEDIA_TYPE_AUDIO ? audioStream_ : videoStream_;
    AVRational timeBase = type == AVMEDIA_TYPE_AUDIO ? timeBaseAudio_ : timeBaseVideo_;
    int64_t& startPts = type == AVMEDIA_TYPE_AUDIO ? startPtsAudio_ : startPtsVideo_;
    if (!fmtCtx_ || !stream) return false;

    AVPacket packet;
    if (av_packet_ref(&packet, pkt) < 0) return false;
    correctPtsDts(&packet, stream, timeBase, startPts);
    packet.stream_index = stream->index;
    const int ret = av_interleaved_write_frame(fmtCtx_, &packet);
    av_packet_unref(&packet);
    return ret >= 0;
}

void Muxer::writeTrailer()
{
    if (type_ == MuxerType::Push) {
        closePublisher();
    } else {
        closeRecorder();
    }
}

void Muxer::close()
{
    closePublisher();
    closeRecorder();
}

void Muxer::closePublisher()
{
    if (publisher_) {
        publisher_->stop();
        publisher_.reset();
    }
    pushState_.store(rtmp::SessionState::Stopped);
    if (type_ == MuxerType::Push) headerWritten_ = false;
}

void Muxer::closeRecorder()
{
    if (!fmtCtx_) return;

    if (headerWritten_) {
        const int ret = av_write_trailer(fmtCtx_);
        if (ret < 0) {
            StatusBarManager::getInstance().showMessage(
                "文件 " + url_ + " 保存失败！", MessageType::Error, 8000);
        } else {
            StatusBarManager::getInstance().showMessage(
                "文件已经保存至 " + url_, MessageType::Info, 8000);
        }
    }
    if (!(fmtCtx_->oformat->flags & AVFMT_NOFILE) && fmtCtx_->pb) {
        avio_closep(&fmtCtx_->pb);
    }
    avformat_free_context(fmtCtx_);
    fmtCtx_ = nullptr;
    audioStream_ = nullptr;
    videoStream_ = nullptr;
    headerWritten_ = false;
}

void Muxer::correctPtsDts(AVPacket* pkt, AVStream* stream,
                          AVRational srcTimebase, int64_t& startPts)
{
    int64_t reference = pkt->dts != AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
    if (startPts == AV_NOPTS_VALUE) startPts = reference;

    if (pkt->pts != AV_NOPTS_VALUE) {
        pkt->pts = av_rescale_q(qMax<int64_t>(0, pkt->pts - startPts),
                                srcTimebase, stream->time_base);
    }
    if (pkt->dts != AV_NOPTS_VALUE) {
        pkt->dts = av_rescale_q(qMax<int64_t>(0, pkt->dts - startPts),
                                srcTimebase, stream->time_base);
    }
    pkt->duration = av_rescale_q(pkt->duration, srcTimebase, stream->time_base);
}

void Muxer::onRequestKeyframe(std::function<void()> callback)
{
    if (publisher_) publisher_->onRequestKeyframe(std::move(callback));
}
