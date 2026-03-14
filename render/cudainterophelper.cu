#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include "cudainterophelper.h"


extern "C"
void copyFrameToGLTexture(
    cudaGraphicsResource* resource,   
    uint8_t* srcDevicePtr,            
    int srcPitch,                     
    int width,                        
    int height                        
) {
    if (!resource || !srcDevicePtr) return;

    cudaGraphicsMapResources(1, &resource, 0);

    cudaArray_t array;
    cudaGraphicsSubResourceGetMappedArray(&array, resource, 0, 0);

    cudaMemcpy2DToArray(
        array,
        0, 0,
        srcDevicePtr,
        srcPitch,
        width * 4,   
        height,
        cudaMemcpyDeviceToDevice
    );

    cudaGraphicsUnmapResources(1, &resource, 0);
}
