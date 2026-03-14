#ifndef NV12_TO_RGBA_H
#define NV12_TO_RGBA_H

#include <stdint.h>
#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// NV12 to RGBA conversion kernel launcher
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

void launchBgraToRgba(
    uint8_t* src,
    uint8_t* dst,
    int pitch,
    int width,
    int height
);
#ifdef __cplusplus
}
#endif

#endif // NV12_TO_RGBA_H
