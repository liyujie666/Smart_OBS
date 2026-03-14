#include "muxer.h"
#include "statusbar/statusbarmanager.h"
#include <QDebug>
#include <QDateTime>
#include <QDir>
Muxer::Muxer()
{}

Muxer::~Muxer()
{
    close();
}

bool Muxer::init(const QString& url,MuxerType type,const QString& format)
{
    qDebug() << "srcUrl" << url;
    OutputTarget target;
    const char* fmt_name = nullptr;
    if(type == MuxerType::Record)
    {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
        QString suffix = QString("%1.%2").arg(timestamp).arg(format);

        QString dirUrl = url;
        if (!dirUrl.isEmpty() && !dirUrl.endsWith(QDir::separator())) {
            dirUrl += QDir::separator(); // 自动添加斜杠（兼容Windows和Linux）
        }

        target.url = dirUrl + suffix; // 拼接路径
        target.type_ = type;
    }
    else
    {
        target.url = url;
        target.type_ = type;
        fmt_name = "flv";
    }

    int ret = avformat_alloc_output_context2(&target.fmtCtx, nullptr, fmt_name, target.url.toUtf8().constData());
    if (ret < 0 || !target.fmtCtx) {
        qDebug() << "Failed to allocate output context for:" << target.url;
        return false;
    }

    if (!(target.fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&target.fmtCtx->pb, target.url.toUtf8().constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            qDebug() << "Failed to open output URL:" << target.url;
            if(type == MuxerType::Record){
                StatusBarManager::getInstance().showMessage("文件路径 " + target.url + " 错误！",MessageType::Warning);
            }else{
                StatusBarManager::getInstance().showMessage("服务器 " + target.url + " 未启动！",MessageType::Warning);
            }
            avformat_free_context(target.fmtCtx);
            return false;
        }else {
            qDebug() << "成功创建文件:" << target.url;  // 确认文件被创建
        }
    }

    targets_.push_back(std::move(target));
    initialized_ = true;
    qDebug() << "Muxer init sucessfully!";
    return true;
}

bool Muxer::writeHeader()
{
    if (!initialized_) {
        qDebug() << "Muxer not initialized";
        return false;
    }

    for (auto& target : targets_) {
        if (!target.headerWritten) {
            int ret = avformat_write_header(target.fmtCtx, nullptr);
            if (ret < 0) {
                qDebug() << "Failed to write header for:" << target.url;
                return false;
            }
            target.headerWritten = true;
        }
    }

    return true;
}

bool Muxer::addStream(AVCodecContext* codecCtx, AVMediaType type)
{
    if (targets_.empty()) {
        qDebug() << "No output targets to add stream to.";
        return false;
    }

    for (auto& target : targets_) {
        AVStream* stream = avformat_new_stream(target.fmtCtx, nullptr);
        if (!stream) {
            qDebug() << "Failed to create stream for:" << target.url;
            return false;
        }

        int ret = avcodec_parameters_from_context(stream->codecpar, codecCtx);
        if (ret < 0) {
            qDebug() << "Failed to copy codec parameters for:" << target.url;
            return false;
        }

        qDebug() << "添加" << (type == AVMEDIA_TYPE_VIDEO ? "视频" : "音频")
                 << "流，编码格式:" << avcodec_get_name(codecCtx->codec_id)
                 << "时间基:" << codecCtx->time_base.num << "/" << codecCtx->time_base.den;

        stream->time_base = codecCtx->time_base;

        if (type == AVMEDIA_TYPE_AUDIO) {
            target.audioStream = stream;
            target.timeBaseAudio = codecCtx->time_base;
        } else if (type == AVMEDIA_TYPE_VIDEO) {
            target.videoStream = stream;
            target.timeBaseVideo = codecCtx->time_base;
        }
    }

    return true;
}

bool Muxer::writePacket(AVPacket* pkt, AVMediaType type)
{

    for (auto& target : targets_) {
        if (!target.headerWritten) {
            qDebug() << "writePacket called before writeHeader for:" << target.url;
            return false;
        }

        AVStream* stream = nullptr;
        AVRational srcTimebase;
        int64_t* startPts = nullptr;

        if (type == AVMEDIA_TYPE_AUDIO) {
            stream = target.audioStream;
            srcTimebase = target.timeBaseAudio;
            startPts = &target.startPtsAudio;
            // qDebug() << "audio pkt pts " << pkt->pts;
            //          << "dts " << pkt->dts
            //          << "duration " << pkt->duration
            //          << " srcTimebase " << srcTimebase.num << "/" << srcTimebase.den
            //          << " target timebase " << stream->time_base.num << "/" << stream->time_base.den;
        } else if (type == AVMEDIA_TYPE_VIDEO) {
            stream = target.videoStream;
            srcTimebase = target.timeBaseVideo;
            startPts = &target.startPtsVideo;
            // qDebug() << "video pkt pts " << pkt->pts
            //          << "dts " << pkt->dts
            //          << "duration " << pkt->duration
            //          << " srcTimebase " << srcTimebase.num << "/" << srcTimebase.den
            //          << " target timebase " << stream->time_base.num << "/" << stream->time_base.den;
        }

        if (!stream) {
            qDebug() << "No stream for type" << type << "in target" << target.url;
            continue;
        }

        AVPacket pktCopy;
        av_packet_ref(&pktCopy, pkt);

        correctPtsDts(&pktCopy, stream, srcTimebase, *startPts);
        pktCopy.stream_index = stream->index;

        int ret = av_interleaved_write_frame(target.fmtCtx, &pktCopy);
        if (ret < 0) {
            // qDebug() << "Failed to write packet to:" << target.url;
        }

        av_packet_unref(&pktCopy);
    }

    return true;
}

void Muxer::writeTrailer()
{
    for (auto& target : targets_) {
        if (target.fmtCtx) {
            if (target.headerWritten) {
                // 写入文件尾，检查错误
                int ret = av_write_trailer(target.fmtCtx);
                QString info;
                if (ret < 0) {
                    qCritical() << "av_write_trailer失败！错误码:" << ret
                                << "描述:" << av_err2str(ret);
                    info = "文件 " + target.url + " 保存失败！";
                    if(target.type_ == MuxerType::Record){
                        StatusBarManager::getInstance().showMessage(info,MessageType::Error,8000);
                    }
                } else {
                    info = "文件已经保存至 "  + target.url;
                    qDebug() << info;
                    if(target.type_ == MuxerType::Record){
                        StatusBarManager::getInstance().showMessage(info,MessageType::Info,8000);
                    }
                }
                target.headerWritten = false;
            }

            // 无论写入尾是否成功，都必须关闭文件句柄
            if (!(target.fmtCtx->oformat->flags & AVFMT_NOFILE) && target.fmtCtx->pb) {
                avio_closep(&target.fmtCtx->pb);  // 关闭IO
            }

            avformat_free_context(target.fmtCtx);  // 释放格式上下文
            target.fmtCtx = nullptr;
        }
    }
    targets_.clear();
    initialized_ = false;
}

void Muxer::close()
{
    writeTrailer();
}

void Muxer::correctPtsDts(AVPacket* pkt, AVStream* stream, AVRational srcTimebase, int64_t& startPts)
{
    // 1. 起始点设为0（相对时间戳从0开始）
    if (startPts == AV_NOPTS_VALUE) {
        startPts = pkt->pts;  // 记录第一帧的绝对pts
        qDebug() << "pkt start pts : " << startPts;
    }

    // 计算相对时间戳（相对于第一帧）
    int64_t relativePts = pkt->pts - startPts;
    int64_t relativeDts = pkt->dts - startPts;

    // 防止相对时间戳为负
    if (relativePts < 0) relativePts = 0;
    if (relativeDts < 0) relativeDts = 0;

    // 2. 确保时间基转换正确（源时间基→目标时间基）

    pkt->pts = av_rescale_q(relativePts, srcTimebase, stream->time_base);
    pkt->dts = av_rescale_q(relativeDts, srcTimebase, stream->time_base);
    pkt->duration = av_rescale_q(pkt->duration, srcTimebase, stream->time_base);

    // 3. 打印转换前后的时间（调试用）
    // qDebug() << (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ? "视频" : "音频")
    //          << "原始pts:" << pkt->pts << "相对pts:" << relativePts
    //          << "转换后pts:" << pkt->pts << "目标时间基:" << stream->time_base.num << "/" << stream->time_base.den;
}
