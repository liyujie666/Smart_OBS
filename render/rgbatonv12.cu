#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <cuda.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include "rgbatonv12.h"

__device__ inline void rgbaToYUV(uchar4 rgba, uint8_t& y, uint8_t& u, uint8_t& v)
{
     float r = rgba.x / 255.0f;
    float g = rgba.y / 255.0f;
    float b = rgba.z / 255.0f;

    // 1. BT.709 矩阵 (Full->Limited)
    float Y =  0.2126f*r + 0.7152f*g + 0.0722f*b;   // 0-1
    float U = -0.1146f*r - 0.3854f*g + 0.5000f*b;   // -0.5~0.5
    float V =  0.5000f*r - 0.4542f*g - 0.0458f*b;   // -0.5~0.5

    // 2. 压到 Studio Swing
    Y = Y * 219.0f +  16.0f;   // 16-235
    U = U * 224.0f + 128.0f;   // 16-240
    V = V * 224.0f + 128.0f;

    // 3. 四舍五入 + 饱和
    y = (uint8_t)fminf(fmaxf(Y + 0.5f, 16.0f), 235.0f);
    u = (uint8_t)fminf(fmaxf(U + 0.5f, 16.0f), 240.0f);
    v = (uint8_t)fminf(fmaxf(V + 0.5f, 16.0f), 240.0f);
}

__global__ void rgbaToNV12Kernel(
    const uchar4* __restrict__ rgba,
    uint8_t* __restrict__ yPlane,
    uint8_t* __restrict__ uvPlane,
    int width, int height,
    size_t pitchRGBA,  
    size_t pitchY,   
    size_t pitchUV    
) {
   int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    // 计算Y分量（每个像素都需要计算）
    const uchar4* rgbaPixel = reinterpret_cast<const uchar4*>(
        reinterpret_cast<const uint8_t*>(rgba) + y * pitchRGBA + x * sizeof(uchar4)
    );
    uchar4 pixel = *rgbaPixel;

    uint8_t yVal, uVal, vVal;
    rgbaToYUV(pixel, yVal, uVal, vVal);
    yPlane[y * pitchY + x] = yVal;

    // 计算UV分量（2x2块的平均值）
    // 只让2x2块中的第一个线程处理该块的UV计算
    if ((x % 2 == 0) && (y % 2 == 0) && (x + 1 < width) && (y + 1 < height)) {
        // 读取2x2块中的四个像素
        uchar4 p00 = *rgbaPixel;
        uchar4 p10 = *(rgbaPixel + 1);
        uchar4 p01 = *reinterpret_cast<const uchar4*>(
            reinterpret_cast<const uint8_t*>(rgbaPixel) + pitchRGBA
        );
        uchar4 p11 = *reinterpret_cast<const uchar4*>(
            reinterpret_cast<const uint8_t*>(rgbaPixel) + pitchRGBA + sizeof(uchar4)
        );

        // 计算四个像素的U和V平均值
        float uSum = 0.0f, vSum = 0.0f;
        rgbaToYUV(p00, yVal, uVal, vVal);
        uSum += uVal; vSum += vVal;
        rgbaToYUV(p10, yVal, uVal, vVal);
        uSum += uVal; vSum += vVal;
        rgbaToYUV(p01, yVal, uVal, vVal);
        uSum += uVal; vSum += vVal;
        rgbaToYUV(p11, yVal, uVal, vVal);
        uSum += uVal; vSum += vVal;

        // 取平均值
        uint8_t uAvg = static_cast<uint8_t>(uSum / 4.0f + 0.5f);
        uint8_t vAvg = static_cast<uint8_t>(vSum / 4.0f + 0.5f);

        // 写入UV平面
        int uvX = x / 2;         
        int uvY = y / 2;         
        int uvIndex = uvY * pitchUV + uvX * 2; 
        
        uvPlane[uvIndex]     = uAvg;
        uvPlane[uvIndex + 1] = vAvg;
    }
}

extern "C" void launchRGBAToNV12(
    CUdeviceptr d_rgba,
    CUdeviceptr d_nv12_y,
    CUdeviceptr d_nv12_uv,
    int width, int height,
    size_t pitchRGBA  
) {
   dim3 block(16, 16);  
    dim3 grid(
        (width + block.x - 1) / block.x,
        (height + block.y - 1) / block.y
    );

    // 修正UV平面的pitch计算，确保内存对齐正确
    size_t pitchY = width;          
    size_t pitchUV = (width + 1) / 2 * 2;  // 确保UV平面宽度是2的倍数

    rgbaToNV12Kernel<<<grid, block>>>(
        reinterpret_cast<const uchar4*>(d_rgba),
        reinterpret_cast<uint8_t*>(d_nv12_y),
        reinterpret_cast<uint8_t*>(d_nv12_uv),
        width, height,
        pitchRGBA, pitchY, pitchUV
    );
}