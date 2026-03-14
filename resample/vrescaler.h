#ifndef VRESCALER_H
#define VRESCALER_H

extern "C"{
#include"libavformat/avformat.h"
#include"libswscale/swscale.h"
#include"libavutil/imgutils.h"
}

struct VideoParams;

class VRescaler
{
public:
    VRescaler();
    ~VRescaler();

    void init(VideoParams* vInParams,VideoParams* vOutParams);
    void rescale(AVFrame* srcFrame,AVFrame** distFrame);
    void release();
private:
    void initSws();
    AVFrame* allocFrame(VideoParams* vPars,AVFrame* srcFrame);
    void printError(int ret);
private:
    SwsContext* swsCtx_ = nullptr;
    VideoParams* srcParams_ = nullptr;
    VideoParams* dstParams_ = nullptr;
    uint8_t* vBuffer = nullptr;
    int maxbufSize = -1;
};

#endif // VRESCALER_H
