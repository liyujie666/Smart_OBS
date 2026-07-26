#ifndef MUXER_H
#define MUXER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <QObject>
#include <QString>
#include <atomic>
#include <functional>
#include <memory>

#include "rtmp/publisher.h"

enum class MuxerType {
    Record,
    Push
};

class Muxer : public QObject
{
    Q_OBJECT
public:
    Muxer();
    ~Muxer();

    bool init(const QString& url, MuxerType type, const QString& format = "mp4");
    bool writeHeader();
    bool addStream(AVCodecContext* codecCtx, AVMediaType type);
    bool writePacket(AVPacket* pkt, AVMediaType type);
    void writeTrailer();
    void close();

    void onRequestKeyframe(std::function<void()> callback);

    bool isPushTarget() const { return type_ == MuxerType::Push; }
    rtmp::SessionState pushState() const { return pushState_.load(); }

private:
    QString url_;
    MuxerType type_ = MuxerType::Record;
    QString format_;
    AVFormatContext* fmtCtx_ = nullptr;
    AVStream* audioStream_ = nullptr;
    AVStream* videoStream_ = nullptr;
    AVRational timeBaseAudio_{0, 1};
    AVRational timeBaseVideo_{0, 1};
    int64_t startPtsAudio_ = AV_NOPTS_VALUE;
    int64_t startPtsVideo_ = AV_NOPTS_VALUE;
    bool headerWritten_ = false;

    std::unique_ptr<rtmp::Publisher> publisher_;
    std::atomic<rtmp::SessionState> pushState_{rtmp::SessionState::Idle};

    bool initRecorder(const QString& format);
    bool initPublisher();
    bool addRecordStream(AVCodecContext* codecCtx, AVMediaType type);
    bool configurePublisherStream(AVCodecContext* codecCtx, AVMediaType type);
    bool writeRecordPacket(AVPacket* pkt, AVMediaType type);
    bool pushPacket(AVPacket* pkt, AVMediaType type);
    void closeRecorder();
    void closePublisher();
    void correctPtsDts(AVPacket* pkt, AVStream* stream, AVRational srcTimebase, int64_t& startPts);
};

#endif // MUXER_H
