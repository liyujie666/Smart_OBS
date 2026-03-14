#ifndef BGRATORGBA_H
#define BGRATORGBA_H

#include "cuda.h"
#include <cuda_runtime.h>
#ifdef __cplusplus
extern "C" {
#endif

// BGRA to RGBA conversion kernel launcher
void launchBGRAToRGBA(CUdeviceptr bgraIn, CUdeviceptr rgbaOut, int width, int height, size_t pitchIn, size_t pitchOut);
#ifdef __cplusplus
}
#endif

#endif // BGRATORGBA_H
