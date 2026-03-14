#ifndef FFMPEGUTILS_H
#define FFMPEGUTILS_H
// FFmpegUtils.h
#include <QString>
#include <QDebug>
extern"C"
{
#include <libavformat/avformat.h>
}


struct StreamInfo {
    bool hasVideo = false;  // 是否有视频流
    bool hasAudio = false;  // 是否有音频流
    AVFormatContext* fmtCtx = nullptr; // 解析后的上下文（需外部释放）
};

class FFmpegUtils {
public:
    // 打开文件并检测流类型
    static StreamInfo probeFileStreams(const QString& filePath) {
        StreamInfo info;
        AVFormatContext* fmtCtx = nullptr;

        // 打开文件
        int ret = avformat_open_input(&fmtCtx, filePath.toUtf8().data(), nullptr, nullptr);
        if (ret < 0) {
            qCritical() << "无法打开文件:" << filePath << av_err2str(ret);
            return info;
        }

        // 获取流信息
        ret = avformat_find_stream_info(fmtCtx, nullptr);
        if (ret < 0) {
            qCritical() << "无法获取流信息:" << av_err2str(ret);
            avformat_close_input(&fmtCtx);
            return info;
        }

        // 遍历流，检测类型
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            AVStream* stream = fmtCtx->streams[i];
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                info.hasVideo = true;
            } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                info.hasAudio = true;
            }
        }

        info.fmtCtx = fmtCtx; // 上下文由外部释放
        return info;
    }
};
#endif // FFMPEGUTILS_H
