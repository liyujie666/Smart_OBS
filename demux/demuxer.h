#ifndef DEMUXER_H
#define DEMUXER_H
#include <QObject>
#include "queue/packetqueue.h"
#include "pool/gloabalpool.h"
extern "C" {
#include <libavformat/avformat.h>
#include<libavutil/cpu.h>
#include<libavdevice/avdevice.h>
#include<libavutil/time.h>
}
class Demuxer
{
public:
    explicit Demuxer(AVFormatContext* fmtCtx);
    explicit Demuxer();
    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;
    ~Demuxer();

    void init(const QString& url,const QString& format,PacketQueue* aPktQueue,PacketQueue* vPktQueue,int type);
    void init(PacketQueue* aPktQueue,PacketQueue* vPktQueue);
    void initDemuxer();
    void setFmtCtx(AVFormatContext* fmtCtx);

    int demux();
    AVStream* getAStream();
    AVStream* getVStream();

    void wakeAllThread();
    void close();

    void stop();
    void pause();
    void resume();
    void seek(int64_t targetUs);
    int getType();
    void waitForSeekCompleted();

private:
    void printError(int ret);

private:
    QString m_url;
    QString m_format;

    AVFormatContext* fmtCtx_ = nullptr;
    AVDictionary* opts = nullptr;
    AVStream* aStream_ = nullptr;
    AVStream* vStream_ = nullptr;

    PacketQueue* aPktQueue_ = nullptr;
    PacketQueue* vPktQueue_ = nullptr;

    AVRational aTimeBase_;
    AVRational vTimeBase_;
    int type_;
    std::mutex demuxMutex_;

    std::atomic<bool> isStopped_ = false;
    std::atomic<bool> isPaused_ = false;
    std::condition_variable pauseCond_;
    std::mutex pauseMutex_;

    std::atomic<bool> isSeeking_{false};       // seek标志位
    int64_t seekTargetTs_{0};                  // 目标seek时间戳（AV_TIME_BASE单位）
    std::mutex seekMutex_;                     // 保护seek参数的锁
    std::condition_variable seekCond_;         // 等待seek完成的条件变量
    bool seekCompleted_{false};                // seek是否完成的标志

};

#endif // DEMUXER_H
