// cuda_mouse_draw.cu
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cuda_runtime.h>
#include <device_launch_parameters.h>

#include <cstdint>
#include <cstdio>   // <stdio.h> 的 C++ 版本，保证 printf 可见

// CUDA内核：绘制鼠标指针（零拷贝实现）
__global__ void drawMouseKernel(
    uint8_t* desktopData, int desktopWidth, int desktopHeight,
    const uint8_t* cursorData, const uint8_t* cursorMask,
    int cursorWidth, int cursorHeight, int hotX, int hotY,
    int cursorX, int cursorY, bool isMonochrome)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= cursorWidth || y >= cursorHeight) return;

    int desktopX = cursorX - hotX + x;
    int desktopY = cursorY - hotY + y;

    if (desktopX < 0 || desktopX >= desktopWidth ||
        desktopY < 0 || desktopY >= desktopHeight) return;

    int desktopIdx = (desktopY * desktopWidth + desktopX) * 4;

    if (isMonochrome && cursorMask) {
        int bitIndex = y * cursorWidth + x;
        int maskIdx = bitIndex / 8;
        int maskBit = bitIndex % 8;
        bool maskValue = (cursorMask[maskIdx] >> (7 - maskBit)) & 1;

        if (maskValue) {
            bool colorValue = (cursorData[maskIdx] >> (7 - maskBit)) & 1;
            uint8_t val = colorValue ? 255 : 0;
            desktopData[desktopIdx + 0] = val; // B
            desktopData[desktopIdx + 1] = val; // G
            desktopData[desktopIdx + 2] = val; // R
            desktopData[desktopIdx + 3] = 255; // A
        }
    }
    else if (!isMonochrome) {
        int cursorIdx = (y * cursorWidth + x) * 4;
        // 假设 cursorData 存储为 RGBA，desktop 存为 BGRA
        desktopData[desktopIdx + 0] = cursorData[cursorIdx + 2]; // B
        desktopData[desktopIdx + 1] = cursorData[cursorIdx + 1]; // G
        desktopData[desktopIdx + 2] = cursorData[cursorIdx + 0]; // R
        desktopData[desktopIdx + 3] = cursorData[cursorIdx + 3]; // A
    }
}

// 主机端函数：启动绘制鼠标的CUDA内核
extern "C" void drawMouseWithCuda(
    uint8_t* desktopData, int desktopWidth, int desktopHeight,
    const uint8_t* cursorData, const uint8_t* cursorMask,
    int cursorWidth, int cursorHeight, int hotX, int hotY,
    int cursorX, int cursorY, bool isMonochrome)
{
    if (!desktopData || !cursorData || cursorWidth <= 0 || cursorHeight <= 0) return;

    dim3 blockDim(16, 16);
    dim3 gridDim((cursorWidth + blockDim.x - 1) / blockDim.x,
                 (cursorHeight + blockDim.y - 1) / blockDim.y);

    drawMouseKernel<<<gridDim, blockDim>>>(
        desktopData, desktopWidth, desktopHeight,
        cursorData, cursorMask,
        cursorWidth, cursorHeight, hotX, hotY,
        cursorX, cursorY, isMonochrome);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        // 用 fprintf 更明确
        fprintf(stderr, "CUDA kernel error: %s\n", cudaGetErrorString(err));
    }
}
