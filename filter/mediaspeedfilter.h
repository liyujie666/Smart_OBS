#ifndef MEDIASPEEDFILTER_H
#define MEDIASPEEDFILTER_H
#include <memory>
#include <string>
#include <vector>
#include <stdint.h>

extern "C" {
#include <libavutil/frame.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/rational.h>
#include <libavutil/channel_layout.h>
}

class MediaSpeedFilter
{
public:
    enum class Speed
    {
        S0_5,  // 0.5倍速
        S1_0,  // 1.0倍速
        S1_5,  // 1.5倍速
        S2_0   // 2.0倍速_
    };

    enum class FrameType{
        VIDEO,
        AUDIO,
        NONE
    };

    MediaSpeedFilter();
    ~MediaSpeedFilter();

    MediaSpeedFilter(const MediaSpeedFilter&) = delete;
    MediaSpeedFilter& operator=(const MediaSpeedFilter&) = delete;

    bool initialize(bool hasVideo,
                    bool hasAudio,
                    const std::tuple<int, int, AVPixelFormat, AVRational>& videoParams = {},
                    const std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational>& audioParams = {},
                    Speed initialSpeed = Speed::S1_0);

    bool changeSpeed(Speed newSpeed);
    int getVideoFrame(AVFrame* srcVFrame,AVFrame* dstVFrame);
    int getAudioFrame(AVFrame* srcAFrame,AVFrame* dstAFrame);
    AVRational getAudioOutTimeBase() const { return m_audioOutTimeBase; }
    std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> getAudioOutputParams() const;
    double getSpeedValue() { return m_speedValue;}
    bool isInitialized();
    void sendEOF();
    void reset();
    void releaseFilters();

private:
    bool initFilterGraph();
    bool initVideoFilters(int width,int height,AVPixelFormat format,AVRational timebase);
    bool initAudioFilters(int sampleRate, AVSampleFormat format,AVChannelLayout ch_layout,AVRational timebase);


    // 滤镜图
    AVFilterGraph* m_filterGraph = nullptr;

    // 视频滤镜
    AVFilterContext* m_videoSrcCtx = nullptr;
    AVFilterContext* m_videoSinkCtx = nullptr;

    // 音频滤镜
    AVFilterContext* m_audioSrcCtx = nullptr;
    AVFilterContext* m_audioSinkCtx = nullptr;

    // 媒体类型
    bool m_hasVideo = false;
    bool m_hasAudio = false;
    bool m_initialized = false;

    // 媒体参数
    std::tuple<int, int, AVPixelFormat, AVRational> m_videoParams;
    std::tuple<int, AVSampleFormat, AVChannelLayout, AVRational> m_audioParams;

    // 倍速
    Speed m_currentSpeed = Speed::S1_0;
    double m_speedValue = 1.0;
    AVRational m_audioOutTimeBase;

    // EOF状态
    bool m_videoEOF = false;
    bool m_audioEOF = false;


};

#endif // MEDIASPEEDFILTER_H
