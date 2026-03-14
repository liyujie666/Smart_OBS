#ifdef _WIN32
#include <windows.h>
#endif

#include <GL/gl.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

__global__ void dummyKernel() {}

int main() {
    dummyKernel<<<1, 1>>>();
    cudaDeviceSynchronize();
    return 0;
}
