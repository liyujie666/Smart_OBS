#ifndef NV12_CONVERSION_H
#define NV12_CONVERSION_H
#include <cuda_runtime.h>

extern "C" void nv12ToRgbaKernel(const unsigned char* srcY, const unsigned char* srcUV,
                                 unsigned char* dst, int width, int height,
                                 int srcYPitch, int srcUVPitch, int dstPitch);
#endif // NV12_CONVERSION_H
