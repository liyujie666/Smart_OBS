#include <algorithm>
#include <cuda_runtime.h>
#include <dxgi1_2.h>
#include "cursorinfo.h"
#include "cudacursorrenderer.h"

// CUDA kernel 声明
__global__ void rotatedColorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorWidth, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
);

__global__ void rotatedMonochromeCursorKernel(
    uchar4* desktop, int desktopWidth,
    const BYTE* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
);

__global__ void colorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorWidth, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
);

__global__ void monochromeCursorKernel(
    uchar4* desktop, int desktopWidth,
    const BYTE* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
);

__global__ void maskedColorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
);

extern "C" bool cudaRenderCursor(
    cudaArray_t desktopArray,
    int desktopWidth,
    int desktopHeight,
    const CursorInfo* cursorInfo,
    bool drawMouse,
    const RECT& displayRect,
    DXGI_MODE_ROTATION rotation
) {
    if (!drawMouse || !cursorInfo || !cursorInfo->visible || !cursorInfo->shapeInfo.buffer) {
        return false;
    }

    int cursorWidth = cursorInfo->shapeInfo.dxgiShape.Width;
    int cursorHeight = cursorInfo->shapeInfo.dxgiShape.Height;

    if (cursorInfo->shapeInfo.dxgiShape.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
        cursorHeight /= 2;
    }

     int localLeft = cursorInfo->position.x - displayRect.left;
    int localTop = cursorInfo->position.y - displayRect.top;

    // 2. 根据旋转状态调整绘制尺寸和坐标（适配逻辑尺寸）
    int adjustedWidth = desktopWidth;
    int adjustedHeight = desktopHeight;
    int adjustedLeft = localLeft;
    int adjustedTop = localTop;

    // 3. 计算有效绘制区域
    int drawWidth = std::min(cursorWidth, adjustedWidth - adjustedLeft);
    int drawHeight = std::min(cursorHeight, adjustedHeight - adjustedTop);
    if (drawWidth <= 0 || drawHeight <= 0 || adjustedLeft < 0 || adjustedTop < 0) {
        return false;
    }


    // 分配线性内存存放桌面数据
    uchar4* dDesktop = nullptr;
    cudaMalloc(&dDesktop, desktopWidth * desktopHeight * sizeof(uchar4));
    cudaMemcpyFromArray(
        dDesktop, desktopArray, 0, 0,
        desktopWidth * desktopHeight * sizeof(uchar4),
        cudaMemcpyDeviceToDevice
    );

    bool success = true;
    dim3 block(16, 16);
    dim3 grid((drawWidth + 15) / 16, (drawHeight + 15) / 16);

    switch (cursorInfo->shapeInfo.dxgiShape.Type) {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR: {
            uchar4* dCursorColor;
            cudaMalloc(&dCursorColor, cursorInfo->shapeInfo.bufferSize);
            cudaMemcpy(dCursorColor, cursorInfo->shapeInfo.buffer,
                       cursorInfo->shapeInfo.bufferSize, cudaMemcpyHostToDevice);

            // 根据旋转状态选择内核
            switch (rotation) {
                case DXGI_MODE_ROTATION_ROTATE90:
                case DXGI_MODE_ROTATION_ROTATE270:
                    // 竖屏调用旋转版本内核
                    rotatedColorCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorColor, cursorWidth, cursorInfo->shapeInfo.dxgiShape.Pitch,
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
                default:
                    // 正常/180度调用原内核
                    colorCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorColor, cursorWidth, cursorInfo->shapeInfo.dxgiShape.Pitch,
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
            }
            cudaFree(dCursorColor);
            break;
        }
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME: {
            BYTE* dCursorMono;
            cudaMalloc(&dCursorMono, cursorInfo->shapeInfo.bufferSize);
            cudaMemcpy(dCursorMono, cursorInfo->shapeInfo.buffer,
                       cursorInfo->shapeInfo.bufferSize, cudaMemcpyHostToDevice);


            // 根据旋转状态选择内核
            switch (rotation) {
                case DXGI_MODE_ROTATION_ROTATE90:
                case DXGI_MODE_ROTATION_ROTATE270:
                    // 竖屏调用旋转版本内核
                    rotatedMonochromeCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorMono, cursorInfo->shapeInfo.dxgiShape.Pitch, 
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
                default:
                    // 正常/180度调用原内核
                    monochromeCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorMono, cursorInfo->shapeInfo.dxgiShape.Pitch,
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
            }           
            

            cudaFree(dCursorMono);
            break;
        }
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR: {
            uchar4* dCursorMasked;
            cudaMalloc(&dCursorMasked, cursorInfo->shapeInfo.bufferSize);
            cudaMemcpy(dCursorMasked, cursorInfo->shapeInfo.buffer,
                       cursorInfo->shapeInfo.bufferSize, cudaMemcpyHostToDevice);


            // 根据旋转状态选择内核
            switch (rotation) {
                case DXGI_MODE_ROTATION_ROTATE90:
                case DXGI_MODE_ROTATION_ROTATE270:
                    // 竖屏调用旋转版本内核
                    rotatedColorCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorMasked, cursorWidth, cursorInfo->shapeInfo.dxgiShape.Pitch,
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
                default:
                    // 正常/180度调用原内核
                    maskedColorCursorKernel<<<grid, block>>>(
                        dDesktop, desktopWidth,
                        dCursorMasked, cursorInfo->shapeInfo.dxgiShape.Pitch,
                        adjustedLeft, adjustedTop, drawWidth, drawHeight
                    );
                    break;
            }    
            
            cudaFree(dCursorMasked);
            break;
        }
        default:
            success = false;
            break;
    }

    if (cudaGetLastError() != cudaSuccess) {
        success = false;
    }

    cudaMemcpyToArray(
        desktopArray, 0, 0, dDesktop,
        desktopWidth * desktopHeight * sizeof(uchar4),
        cudaMemcpyDeviceToDevice
    );

    cudaFree(dDesktop);
    return success;
}

// CUDA kernel 实现
__global__ void rotatedColorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorWidth, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= drawWidth || y >= drawHeight) return;

    // 90度旋转：鼠标X→Y，Y→(cursorWidth - X - 1)
    int rotatedCursorX = y;
    int rotatedCursorY = cursorWidth - x - 1;
    int cursorIdx = rotatedCursorY * (cursorPitch / sizeof(uchar4)) + rotatedCursorX;
    uchar4 cursorPixel = cursor[cursorIdx];

    if (cursorPixel.w == 0) return;

    // 颜色通道转换（同原内核）
    uchar4 pixel;
    pixel.x = cursorPixel.z;
    pixel.y = cursorPixel.y;
    pixel.z = cursorPixel.x;
    pixel.w = cursorPixel.w;

    // 桌面坐标无需旋转（已在坐标转换阶段处理）
    int desktopIdx = (top + y) * desktopWidth + (left + x);
    desktop[desktopIdx] = pixel;
}

__global__ void rotatedMonochromeCursorKernel(
    uchar4* desktop, int desktopWidth,
    const BYTE* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= drawWidth || y >= drawHeight) return;

    // 90/270度旋转时，鼠标坐标转换（根据实际旋转方向调整）
    // 这里以90度为例：X→Y，Y→(原宽度 - X - 1)
    int rotatedX = y;
    int rotatedY = drawWidth - x - 1;  // 基于绘制宽度反向计算

    // 单色鼠标数据读取（与原内核逻辑一致，但使用旋转后的坐标）
    int byteOffset = (rotatedX / 8) + rotatedY * cursorPitch;
    BYTE mask = 0x80 >> (rotatedX % 8);
    BYTE andMask = (cursor[byteOffset] & mask) ? 0xFF : 0x00;
    BYTE xorMask = (cursor[byteOffset + drawHeight * cursorPitch] & mask) ? 0xFF : 0x00;

    // 桌面坐标使用调整后的left/top（已适配旋转）
    int desktopIdx = (top + y) * desktopWidth + (left + x);
    uchar4& pixel = desktop[desktopIdx];

    // 单色鼠标绘制逻辑（保持不变）
    pixel.x = (pixel.x & andMask) ^ xorMask;
    pixel.y = (pixel.y & andMask) ^ xorMask;
    pixel.z = (pixel.z & andMask) ^ xorMask;
}


__global__ void colorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorWidth, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= drawWidth || y >= drawHeight) return;

    int cursorIdx = y * (cursorPitch / sizeof(uchar4)) + x;
    uchar4 cursorPixel = cursor[cursorIdx];

    if (cursorPixel.w == 0) return; // 跳过透明像素

    uchar4 pixel;
    pixel.x = cursorPixel.z;
    pixel.y = cursorPixel.y;
    pixel.z = cursorPixel.x;
    pixel.w = cursorPixel.w;

    int desktopIdx = (top + y) * desktopWidth + (left + x);
    desktop[desktopIdx] = pixel;
}

__global__ void monochromeCursorKernel(
    uchar4* desktop, int desktopWidth,
    const BYTE* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= drawWidth || y >= drawHeight) return;

    int byteOffset = (x / 8) + y * cursorPitch;
    BYTE mask = 0x80 >> (x % 8);
    BYTE andMask = (cursor[byteOffset] & mask) ? 0xFF : 0x00;
    BYTE xorMask = (cursor[byteOffset + drawHeight * cursorPitch] & mask) ? 0xFF : 0x00;

    int desktopIdx = (top + y) * desktopWidth + (left + x);
    uchar4& pixel = desktop[desktopIdx];

    pixel.x = (pixel.x & andMask) ^ xorMask;
    pixel.y = (pixel.y & andMask) ^ xorMask;
    pixel.z = (pixel.z & andMask) ^ xorMask;
}

__global__ void maskedColorCursorKernel(
    uchar4* desktop, int desktopWidth,
    const uchar4* cursor, int cursorPitch,
    int left, int top, int drawWidth, int drawHeight
) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= drawWidth || y >= drawHeight) return;

    int cursorIdx = y * (cursorPitch / sizeof(uchar4)) + x;
    uchar4 cursorPixel = cursor[cursorIdx];
    BYTE alpha = cursorPixel.w;

    int desktopIdx = (top + y) * desktopWidth + (left + x);
    uchar4& desktopPixel = desktop[desktopIdx];

    if (alpha == 0) {
        desktopPixel.x = cursorPixel.z;
        desktopPixel.y = cursorPixel.y;
        desktopPixel.z = cursorPixel.x;
    } else {
        desktopPixel.x ^= cursorPixel.z;
        desktopPixel.y ^= cursorPixel.y;
        desktopPixel.z ^= cursorPixel.x;
    }
}
