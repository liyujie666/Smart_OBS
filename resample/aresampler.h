#ifndef ARESAMPLER_H
#define ARESAMPLER_H
extern "C"{
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libavutil/channel_layout.h"
}
#include "atomic"
class AudioParams;

class AResampler
{
public:
    explicit AResampler();
    ~AResampler();

    void init(AudioParams* src, AudioParams* dst);
    void resample(AVFrame* srcFrame, AVFrame** dstFrame);
    void release();

private:
    void initSwr();
    AVFrame* allocFrame(AudioParams* aPars, int nbSamples, AVFrame* srcFrame);
    void printError(int ret);

private:
    SwrContext* swrCtx = nullptr;
    AudioParams* srcParams = nullptr;
    AudioParams* dstParams = nullptr;

    AVChannelLayout srcLayout;
    AVChannelLayout dstLayout;

};

#endif // ARESAMPLER_H
