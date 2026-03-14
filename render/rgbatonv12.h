#ifndef RGBATONV12_H
#define RGBATONV12_H

#include <cuda.h>
#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

// CUDA kernel 启动函数：RGBA → NV12
// 参数说明：
//   d_rgba     - 输入 RGBA 图像 (uchar4 数组)，pitch = width * 4
//   d_nv12_y   - 输出 Y 分量，大小 width * height
//   d_nv12_uv  - 输出 UV 交错分量，大小 width * height / 2
//   width      - 宽度
//   height     - 高度
void launchRGBAToNV12(
    CUdeviceptr d_rgba,
    CUdeviceptr d_nv12_y,
    CUdeviceptr d_nv12_uv,
    int width, int height,
    size_t pitchRGBA
    );

#ifdef __cplusplus
}
#endif

#endif // RGBATONV12_H
