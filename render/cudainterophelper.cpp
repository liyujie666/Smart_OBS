// cudainterophelper.cpp
#include "cudainterophelper.h"

CudaInteropHelper::CudaInteropHelper() {
    impl_ = std::make_unique<CudaInteropHelperImpl>();
}

CudaInteropHelper::~CudaInteropHelper()
{
    release();
}

void CudaInteropHelper::initInterop(int w, int h) {
    impl_->initInterop(w, h);
}

void CudaInteropHelper::uploadNV12ToGL(CUdeviceptr y, CUdeviceptr uv, int pitchY, int pitchUV) {
    impl_->uploadNV12ToGL(y, uv, pitchY, pitchUV);
}

void CudaInteropHelper::uploadBGRAToGL(cudaArray_t cuArray)
{
    impl_->uploadBGRAToGL(cuArray);
}

bool CudaInteropHelper::registerFboTexture(GLuint texId, int w, int h)
{
    return impl_->registerFboTexture(texId,w,h);
}

cudaArray_t CudaInteropHelper::mapFboCudaArray()
{
    return impl_->mapFboCudaArray();
}

void CudaInteropHelper::unmapFboCudaArray()
{
    impl_->unmapFboCudaArray();
}

int CudaInteropHelper::getDeviceId() const
{
   return impl_->targetDevice;
}

bool CudaInteropHelper::isInitialized() const {
    return impl_->isInitialized();
}

GLuint CudaInteropHelper::texture() const {
    return impl_->texture();
}

int CudaInteropHelper::width() const
{
    return impl_->getWidth();
}

int CudaInteropHelper::height() const
{
    return impl_->getHeight();
}

void CudaInteropHelper::waitForOperationsToComplete()
{
    impl_->waitForOperationsToComplete();
}

void CudaInteropHelper::release()
{
    // QOpenGLContext* currentCtx = QOpenGLContext::currentContext();
    // if (currentCtx) {
    //     currentCtx->makeCurrent(currentCtx->surface());
    // }
    impl_->release();
}
