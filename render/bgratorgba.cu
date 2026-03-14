#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

__global__ void bgraToRgbaKernel(uint8_t* bgra, uint8_t* rgba, int width, int height, size_t pitchIn, size_t pitchOut)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;

    if (x >= width || y >= height) return;

    uint8_t* inPixel = bgra + y * pitchIn + x * 4;
    uint8_t* outPixel = rgba + y * pitchOut + x * 4;

    outPixel[0] = inPixel[2];  // R
    outPixel[1] = inPixel[1];  // G
    outPixel[2] = inPixel[0];  // B
    outPixel[3] = inPixel[3];  // A
}

extern "C" void launchBGRAToRGBA(CUdeviceptr bgraIn, CUdeviceptr rgbaOut, int width, int height, size_t pitchIn, size_t pitchOut)
{
    dim3 block(16, 16);
    dim3 grid((width + block.x - 1) / block.x, (height + block.y - 1) / block.y);

    bgraToRgbaKernel<<<grid, block>>>(reinterpret_cast<uint8_t*>(bgraIn),
                                      reinterpret_cast<uint8_t*>(rgbaOut),
                                      width, height, pitchIn, pitchOut);
}
