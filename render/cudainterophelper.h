#ifndef CUDA_INTEROP_HELPER_H
#define CUDA_INTEROP_HELPER_H
#include "cudainterophelperimpl.h"
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>

struct cudaGraphicsResource;

class CudaInteropHelper {
public:
    CudaInteropHelper();
    ~CudaInteropHelper();
    void initInterop(int w, int h);
    void uploadNV12ToGL(CUdeviceptr y, CUdeviceptr uv, int pitchY, int pitchUV);
    void uploadBGRAToGL(cudaArray_t cuArray);

    bool registerFboTexture(GLuint texId, int w, int h);
    cudaArray_t mapFboCudaArray();
    void unmapFboCudaArray();
    int getDeviceId() const;
    bool isInitialized() const;
    GLuint texture() const;
    int width() const;
    int height() const;
    void waitForOperationsToComplete();
    void release();
private:
private:
    std::unique_ptr<CudaInteropHelperImpl> impl_;
};


#endif // CUDA_INTEROP_HELPER_H
