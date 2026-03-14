#ifndef AENCODER_H
#define AENCODER_H
#include <QObject>
#include "muxer/muxermanager.h"
extern"C"
{
#include <libavcodec/avcodec.h>
}
class PacketQueue;

struct AudioEncodeConfig{
    int sampleRate = 48000;
    int nbChannels = 2;
    int bitRate = 128000;
    enum AVSampleFormat sampleFmt = AV_SAMPLE_FMT_FLTP;
};

class AEncoder {
public:

    AEncoder(PacketQueue* pktQueue);
    ~AEncoder();

    bool init(const AudioEncodeConfig& config);
    bool encode(AVFrame* frame); // 每次从AudioMixer拉一帧，编码并入队
    bool encodeFlushFrame(AVFrame* frame,MuxerManager* muxerManager);
    bool sendSingleFrame(AVFrame* frame);
    bool copyFrameData(AVFrame* dst, AVFrame* src, int offset_samples, int nb_samples);
    void flush(MuxerManager* muxerManager = nullptr);
    void close();

    const AudioEncodeConfig& getConfig() const { return config_; }
    AVCodecContext* getCodecContext() const { return codecCtx_; }

    // 获取数据包队列
    PacketQueue* getPacketQueue() { return aEnPktQueue_; }

private:
    AVCodecContext* codecCtx_ = nullptr;
    const AVCodec* codec_ = nullptr;
    AVFrame* frame_ = nullptr;
    PacketQueue* aEnPktQueue_ = nullptr;
    AudioEncodeConfig config_;

    bool is_initialized_ = false;
};

#endif // AENCODER_H
