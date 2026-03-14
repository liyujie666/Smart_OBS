#include "resample/vrescaler.h"
#include "decoder/vdecoder.h"
#include "pool/gloabalpool.h"
#include <QDebug>
VRescaler::VRescaler() {}

VRescaler::~VRescaler()
{
    release();
}

void VRescaler::init(VideoParams *srcParams, VideoParams *dstParams)
{
    release();

    srcParams_ = new VideoParams();
    memcpy(srcParams_,srcParams,sizeof(VideoParams));

    dstParams_ = new VideoParams();
    memcpy(dstParams_,dstParams,sizeof(VideoParams));

    qDebug() << "============ input Video info ==============";
    qDebug() << "width  = " << srcParams_->width;
    qDebug() << "height = " << srcParams_->height;
    qDebug() << "framerate = " << srcParams_->frameRate.num << "/" << srcParams_->frameRate.den;
    qDebug() << "format = " << av_get_pix_fmt_name(srcParams_->pixFmt);


    qDebug() << "============ output Video info ==============";
    qDebug() << "width  = " << dstParams_->width;
    qDebug() << "height = " << dstParams_->height;
    qDebug() << "framerate = " << dstParams_->frameRate.num << "/" << dstParams_->frameRate.den;
    qDebug() << "format = " << av_get_pix_fmt_name(dstParams_->pixFmt);

    initSws();


}

void VRescaler::rescale(AVFrame *srcFrame, AVFrame **dstFrame)
{
    *dstFrame = allocFrame(dstParams_,srcFrame);
    if(dstFrame == nullptr)
    {
        return;
    }

    sws_scale(swsCtx_,srcFrame->data,srcFrame->linesize,0,srcFrame->height,(*dstFrame)->data,(*dstFrame)->linesize);

}


void VRescaler::initSws()
{
    swsCtx_ = sws_getContext(srcParams_->width,srcParams_->height,srcParams_->pixFmt,
                             dstParams_->width,dstParams_->height,dstParams_->pixFmt,
                             SWS_FAST_BILINEAR,nullptr,nullptr,nullptr);
    if(!swsCtx_)
    {
        qDebug() << "swsCtx_ init failed";
        return;
    }
}

AVFrame* VRescaler::allocFrame(VideoParams *vParams, AVFrame *srcFrame)
{
    AVFrame* frame = GlobalPool::getFramePool().get();
    int bufSize = av_image_get_buffer_size(vParams->pixFmt,vParams->width,vParams->height,1);

    if(bufSize > maxbufSize){
        maxbufSize = bufSize;
        if(vBuffer){
            av_freep(&vBuffer);
        }
        vBuffer = static_cast<uint8_t*>(av_mallocz(bufSize));
        if(!vBuffer){
            GlobalPool::getFramePool().recycle(frame);
            qDebug() << "malloc vBuffer error!";
            return nullptr;
        }
    }

    // 初始化帧缓冲区
    int ret = av_image_fill_arrays(frame->data,frame->linesize,vBuffer,vParams->pixFmt,vParams->width,vParams->height,1);
    if(ret < 0)
    {
        printError(ret);
        GlobalPool::getFramePool().recycle(frame);
        return nullptr;
    }

    frame->width = vParams->width;
    frame->height = vParams->height;
    frame->format = vParams->pixFmt;
    frame->pts = srcFrame->pts;

    return frame;

}

void VRescaler::release()
{
    if(swsCtx_){
        sws_freeContext(swsCtx_);
    }
    if(srcParams_){
        delete srcParams_;
        srcParams_ = nullptr;
    }
    if(dstParams_){
        delete dstParams_;
        dstParams_ = nullptr;
    }
    if(vBuffer){
        av_freep(&vBuffer);
        vBuffer = nullptr;
    }
}


void VRescaler::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        qDebug() << "Unknow Error!";
    }
    else{
        qDebug() << "Error:" << errorBuffer;
    }
}
