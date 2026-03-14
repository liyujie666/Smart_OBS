#include "aresampler.h"
#include "decoder/adecoder.h"
#include "pool/gloabalpool.h"
#include <QDebug>
AResampler::AResampler() {}

AResampler::~AResampler()
{
    release();
}

void AResampler::init(AudioParams *src, AudioParams *dst)
{

    release();

    srcParams = new AudioParams();
    memcpy(srcParams,src,sizeof(AudioParams));

    dstParams = new AudioParams();
    memcpy(dstParams,dst,sizeof(AudioParams));

    initSwr();

}

void AResampler::resample(AVFrame *srcFrame, AVFrame **dstFrame)
{
    // 获取重采样延迟
    int64_t delaySamples = swr_get_delay(swrCtx, srcParams->sampleRate);
    // 计算最大输出样本数
    int maxNbSamples = swr_get_out_samples(swrCtx, srcFrame->nb_samples + delaySamples);

    *dstFrame = allocFrame(dstParams, maxNbSamples, srcFrame);
    if (!*dstFrame) {
        qDebug() << "av_frame_alloc error!";
        return;
    }

    // 调整输出帧的 PTS
    AVRational srcTimeBase = {1, srcParams->sampleRate};
    AVRational dstTimeBase = {1, dstParams->sampleRate};

    (*dstFrame)->pts = av_rescale_q(
        srcFrame->pts * srcFrame->nb_samples + delaySamples,
        srcTimeBase,
        dstTimeBase
        );

    int samples = swr_convert(
        swrCtx,
        (*dstFrame)->data,
        maxNbSamples,
        (const uint8_t **)srcFrame->data,
        srcFrame->nb_samples
        );
    if (samples < 0) {
        printError(samples);
        swr_free(&swrCtx);
        return;
    }

    // 更新输出帧的实际样本数
    (*dstFrame)->nb_samples = samples;

}


void AResampler::initSwr()
{
    // 初始化默认通道布局
    av_channel_layout_default(&srcLayout, srcParams->nbChannels);
    av_channel_layout_default(&dstLayout, dstParams->nbChannels);

    int ret = swr_alloc_set_opts2(&swrCtx,
                                  &dstLayout, dstParams->format, dstParams->sampleRate,
                                  &srcLayout, srcParams->format, srcParams->sampleRate,
                                  0, nullptr);
    if (ret < 0) {
        qDebug() << "Swr Alloc Set Opts Fail !";
        printError(ret);
        swrCtx = nullptr;
    }

    // 清理通道布局资源
    av_channel_layout_uninit(&srcLayout);
    av_channel_layout_uninit(&dstLayout);

    if (!swrCtx) {
        qDebug() << "initSwr error!";
        return;
    }

    ret = swr_init(swrCtx);
    if (ret < 0) {
        printError(ret);
        swr_free(&swrCtx);
        return;
    }
}

AVFrame *AResampler::allocFrame(AudioParams *aPars, int nbSamples, AVFrame *srcFrame)
{
    AVFrame* frame = GlobalPool::getFramePool().get();
    if(!frame) return nullptr;

    frame->format = aPars->format;
    frame->sample_rate = aPars->sampleRate;
    frame->nb_samples = nbSamples;
    av_channel_layout_default(&frame->ch_layout, aPars->nbChannels);

    if (srcFrame) {
        frame->pts = srcFrame->pts;
    }

    int ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        printError(ret);
        GlobalPool::getFramePool().recycle(frame);
        return nullptr;
    }

    return frame;

}

void AResampler::release()
{
    if (swrCtx) {
        swr_free(&swrCtx);
    }
    if (srcParams) {
        delete srcParams;
        srcParams = nullptr;
    }
    if (dstParams) {
        delete dstParams;
        dstParams = nullptr;
    }
}


void AResampler::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret, errorBuffer, sizeof(errorBuffer));
    if (res < 0) {
        qDebug() << "Unknown Error!";
    } else {
        qDebug() << "Error: " << errorBuffer;
    }
}
