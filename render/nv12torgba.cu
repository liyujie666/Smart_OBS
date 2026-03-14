#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include "nv12torgba.h"

__device__ uint8_t clamp(float val) {
    return (val < 0.0f) ? 0 : ((val > 255.0f) ? 255 : static_cast<uint8_t>(val));
}

__global__ void NV12ToRGBAKernel(
    uint8_t* yPlane,
    uint8_t* uvPlane,
    uint8_t* rgba,
    int width,
    int height,
    int pitchY,
    int pitchUV,
    int pitchRGBA
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    int yIndex = y * pitchY + x;

    // NV12：UV 是交错排列，按 2x2 对应 Y
    int uv_x = x & ~1;  // 偶数对齐
    int uvIndex = (y / 2) * pitchUV + uv_x;

    float Y = static_cast<float>(yPlane[yIndex]);
    float U = static_cast<float>(uvPlane[uvIndex]) - 128.0f;
    float V = static_cast<float>(uvPlane[uvIndex + 1]) - 128.0f;

    float R = Y + 1.28033f * V;
    float G = Y - 0.21482f * U - 0.38059f * V;
    float B = Y + 2.12798f * U;  

    int rgbaIndex = y * pitchRGBA + x * 4;
    rgba[rgbaIndex + 0] = clamp(R);
    rgba[rgbaIndex + 1] = clamp(G);
    rgba[rgbaIndex + 2] = clamp(B);
    rgba[rgbaIndex + 3] = 255; // alpha
}

extern "C"
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
) {
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x,
              (height + block.y - 1) / block.y);

    NV12ToRGBAKernel<<<grid, block, 0, stream>>>(
        yPlane, uvPlane, rgbaOut, width, height,
        pitchY, pitchUV, pitchRGBA
    );
}