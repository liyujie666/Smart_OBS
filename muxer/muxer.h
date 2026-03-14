#ifndef MUXER_H
#define MUXER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <QString>
#include <vector>

enum class MuxerType {
    Record,
    Push
};
class Muxer
{
public:
    Muxer();
    ~Muxer();

    // 初始化输出，支持多个调用，先初始化不写头
    bool init(const QString& url,MuxerType type,const QString& format = "mp4");

    // 写入所有输出的头
    bool writeHeader();

    // 添加音视频流
    bool addStream(AVCodecContext* codecCtx, AVMediaType type);

    // 写入包，要求先写头
    bool writePacket(AVPacket* pkt, AVMediaType type);

    // 写入尾部，关闭释放
    void writeTrailer();

    // 关闭并释放资源
    void close();

private:
    struct OutputTarget {
        QString url;
        QString format;
        AVFormatContext* fmtCtx = nullptr;
        AVStream* audioStream = nullptr;
        AVStream* videoStream = nullptr;
        AVRational timeBaseAudio;
        AVRational timeBaseVideo;
        int64_t startPtsAudio = AV_NOPTS_VALUE;
        int64_t startPtsVideo = AV_NOPTS_VALUE;
        bool headerWritten = false;
        MuxerType type_;
    };

    std::vector<OutputTarget> targets_;
    bool initialized_ = false;

    void correctPtsDts(AVPacket* pkt, AVStream* stream, AVRational srcTimebase, int64_t& startPts);
};

#endif // MUXER_H
