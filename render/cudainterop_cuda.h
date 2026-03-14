#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#ifndef CUDA_INTEROP_HELPER_H
#define CUDA_INTEROP_HELPER_H


#include <stdint.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

#ifdef __cplusplus
extern "C" {
#endif

void copyFrameToGLTexture(
    cudaGraphicsResource* resource,
    uint8_t* srcDevicePtr,
    int srcPitch,
    int width,
    int height
    );

// 你原来的 CUDA kernel wrapper 声明也写这里
void launchNV12ToRGBA(
    uint8_t* yPlane,
    uint8_t* uvPlane,
    uint8_t* rgbaOut,
    int width,
    int height,
    int pitchY,
    int pitchUV,
    int pitchRGBA,
    cudaStream_t stream
    );

#ifdef __cplusplus
}
#endif

#endif // CUDA_INTEROP_CUDA_H
