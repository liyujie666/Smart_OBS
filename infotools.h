#ifndef INFOTOOLS_H
#define INFOTOOLS_H

#include <QOpenGLFunctions>
#include <cuda.h>
#include <QMetaType>
#include <cuda_runtime_api.h>

// 状态控制
enum class Status{
    Running,
    Paused,
    Stopped
};

// 视频输入源
enum class VideoSourceType {
    Camera,
    Desktop,
    Media,
    Text,
    Color
};

// 音频输入源
enum class AudioSourceType {
    Microphone,
    Desktop,
    Media
};

// 硬解帧类型
enum class FrameFormat {
    NV12,
    BGRA
};


// Cuda渲染帧
struct CudaFrameInfo {
    FrameFormat format;
    VideoSourceType type;
    union {
        struct {
            CUdeviceptr yPlane;
            CUdeviceptr uvPlane;
        };                          // 用于 NV12

        cudaArray_t bgraArray;          // 用于 BGRA
    };

    int pitchY;
    int pitchUV;
    int width;
    int height;
    int sourceId;
    int sceneId;
    int priority;                   // 优先级，值越大越在上层
    int64_t timestamp;              // 全局时间戳
    int64_t pts;
    bool skip = false;

};
Q_DECLARE_METATYPE(CudaFrameInfo)

// 桌面采集方式
enum class DesktopCaptureType {
    FFmpegGdiGrab,
    DXGI
};

#endif // INFOTOOLS_H
