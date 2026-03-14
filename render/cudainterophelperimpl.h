#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <QOpenGLFunctions>
#ifndef CUDAINTEROPHELPERIMPL_H
#define CUDAINTEROPHELPERIMPL_H
#include <QDebug>
#include <cuda.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <mutex>

extern "C" void launchNV12ToRGBA(
    uint8_t* yPlane,
    uint8_t* uvPlane,
    uint8_t* rgbaOut,
    int width,
    int height,
    int pitchY,
    int pitchUV,
    int pitchRGBA,
    cudaStream_t stream);

extern "C" void launchBGRAToRGBA(
    CUdeviceptr bgraIn,
    CUdeviceptr rgbaOut,
    int width,
    int height,
    size_t pitchIn,
    size_t pitchOut
);

class CudaInteropHelperImpl {
public:
    int width = 0;
    int height = 0;
    GLuint glTex = 0;
    CUtexObject cudaTex_ = 0;
    CUgraphicsResource cuRes_ = nullptr;            // 若未来注册 PBO 可用
    cudaGraphicsResource* cudaResource = nullptr;
    bool initialized = false;

    uint8_t* rgbaBuffer = nullptr;
    uint8_t* bgraBuffer = nullptr;
    size_t pitchRGBA = 0;
    size_t pitchBGRA = 0;
    int cachedW = 0;
    int cachedH = 0;

    // FBO
    cudaGraphicsResource* fboCudaResource = nullptr;
    GLuint fboTex = 0;
    int fboWidth = 0;
    int fboHeight = 0;
    int targetDevice = 0;
    std::mutex resourceMutex_;
    cudaEvent_t releaseEvent = nullptr;


    CudaInteropHelperImpl() {
        cudaEventCreate(&releaseEvent); // 初始化事件
    }

    ~CudaInteropHelperImpl() {
        cudaEventDestroy(releaseEvent); // 销毁事件
        release();
    }


    // 新增：等待所有 CUDA 操作完成
    void waitForOperationsToComplete() {
        cudaEventSynchronize(releaseEvent);
    }

    // 新增：标记当前 CUDA 操作完成
    void markOperationComplete() {
        cudaEventRecord(releaseEvent, 0);
    }

    void initInterop(int w, int h)
    {
        if (initialized && (width != w || height != h)) {
            release(); // 调用现有release释放旧资源
        }
        if (initialized) return;

        width = w;
        height = h;

        QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
        f->glGenTextures(1, &glTex);
        f->glBindTexture(GL_TEXTURE_2D, glTex);
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glBindTexture(GL_TEXTURE_2D, 0);

        cudaError_t err = cudaGraphicsGLRegisterImage(&cudaResource, glTex, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
        if (err != cudaSuccess) {
            qWarning() << "cudaGraphicsGLRegisterImage failed:" << cudaGetErrorString(err);
            return;
        }
        qDebug() << "initInterop success, glTex=" << glTex;
        initialized = true;
    }

    void uploadNV12ToGL(CUdeviceptr yPlane, CUdeviceptr uvPlane, int pitchY, int pitchUV)
    {
        if (!initialized) return;
        cudaError_t err = cudaGraphicsMapResources(1, &cudaResource, 0);
        if (err != cudaSuccess) {
            qWarning() << "uploadNV12ToGL cudaGraphicsMapResources failed:" << cudaGetErrorString(err);
            return;
        }
        cudaArray_t texArray = nullptr;
        err = cudaGraphicsSubResourceGetMappedArray(&texArray, cudaResource, 0, 0);
        if (err != cudaSuccess || texArray == nullptr) {
            qWarning() << "cudaGraphicsSubResourceGetMappedArray failed:" << cudaGetErrorString(err);
            cudaGraphicsUnmapResources(1, &cudaResource, 0);
            return;
        }

        if (!rgbaBuffer || width != cachedW || height != cachedH) {
            if (rgbaBuffer) cudaFree(rgbaBuffer);
            err = cudaMallocPitch(&rgbaBuffer, (size_t*)&pitchRGBA, width * 4, height);
            if (err != cudaSuccess) {
                qWarning() << "cudaMallocPitch failed:" << cudaGetErrorString(err);
                cudaGraphicsUnmapResources(1, &cudaResource, 0);
                return;
            }
        }

        launchNV12ToRGBA(
            (uint8_t*)yPlane,
            (uint8_t*)uvPlane,
            rgbaBuffer,
            width,
            height,
            pitchY,
            pitchUV,
            pitchRGBA,
            0);
        err = cudaMemcpy2DToArray(texArray, 0, 0, rgbaBuffer, pitchRGBA, width * 4, height, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            qWarning() << "cudaMemcpy2DToArray failed:" << cudaGetErrorString(err);
        }

        // 确保 CUDA kernel 和 memcpy 都完成
        cudaStreamSynchronize(0);

        cudaGraphicsUnmapResources(1, &cudaResource, 0);
        markOperationComplete(); // 你已有的事件标记


    }


    bool uploadBGRAToGL(cudaArray_t srcArray)
    {
        std::lock_guard<std::mutex> lock(resourceMutex_); // 加锁保护全程
        if (!initialized || !srcArray || !cudaResource) { // 新增：检查 cudaResource 有效性
            qWarning() << "uploadBGRAToGL: invalid state (initialized=" << initialized
                       << ", srcArray=" << srcArray << ", cudaResource=" << cudaResource << ")";
            return false;
        }

        // 1. Map GL 纹理资源（cudaGraphicsGLRegisterImage 注册的）
        cudaError_t err = cudaGraphicsMapResources(1, &cudaResource, 0);
        if (err != cudaSuccess) {
            qWarning() << "uploadBGRAToGL cudaGraphicsMapResources failed:" << cudaGetErrorString(err);
            return false;
        }

        cudaArray_t glTexArray = nullptr;
        err = cudaGraphicsSubResourceGetMappedArray(&glTexArray, cudaResource, 0, 0);
        if (err != cudaSuccess || !glTexArray) {
            qWarning() << "cudaGraphicsSubResourceGetMappedArray failed:" << cudaGetErrorString(err);
            cudaGraphicsUnmapResources(1, &cudaResource, 0);
            return false;
        }

        // 2. 分配临时线性内存：BGRA → RGBA 转换用
        if (!bgraBuffer || width != cachedW || height != cachedH) {
            if (bgraBuffer) cudaFree(bgraBuffer);
            if (rgbaBuffer) cudaFree(rgbaBuffer);

            err = cudaMallocPitch(&bgraBuffer, &pitchBGRA, width * 4, height);
            if (err != cudaSuccess) {
                qWarning() << "cudaMallocPitch for bgraBuffer failed:" << cudaGetErrorString(err);
                cudaGraphicsUnmapResources(1, &cudaResource, 0);
                return false;
            }

            err = cudaMallocPitch(&rgbaBuffer, &pitchRGBA, width * 4, height);
            if (err != cudaSuccess) {
                qWarning() << "cudaMallocPitch for rgbaBuffer failed:" << cudaGetErrorString(err);
                cudaGraphicsUnmapResources(1, &cudaResource, 0);
                return false;
            }

            cachedW = width;
            cachedH = height;
        }

        // 3. 拷贝 cudaArray(BGRA) → 线性 BGRA Buffer
        err = cudaMemcpy2DFromArray(bgraBuffer, pitchBGRA, srcArray, 0, 0, width * 4, height, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            qWarning() << "cudaMemcpy2DFromArray failed:" << cudaGetErrorString(err);
            cudaGraphicsUnmapResources(1, &cudaResource, 0);
            return false;
        }

        // 4. 调用 CUDA kernel：BGRA → RGBA
        launchBGRAToRGBA((CUdeviceptr)bgraBuffer, (CUdeviceptr)rgbaBuffer, width, height,pitchBGRA,pitchRGBA);

        // 5. 拷贝 RGBA 线性 Buffer → OpenGL 纹理 Array
        err = cudaMemcpy2DToArray(glTexArray, 0, 0, rgbaBuffer, pitchRGBA, width * 4, height, cudaMemcpyDeviceToDevice);
        if (err != cudaSuccess) {
            qWarning() << "cudaMemcpy2DToArray failed:" << cudaGetErrorString(err);
            cudaGraphicsUnmapResources(1, &cudaResource, 0);
            return false;
        }

        // 6. Unmap
        cudaStreamSynchronize(0);
        cudaGraphicsUnmapResources(1, &cudaResource, 0);

        markOperationComplete();
        return true;
    }



    // 注册FBO为Cuda资源
    bool registerFboTexture(GLuint fboTexId, int width, int height)
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        if (fboTexId == 0) return false;

        // 如果已有，先注销
        if (fboCudaResource) {
            cudaGraphicsUnregisterResource(fboCudaResource);
            fboCudaResource = nullptr;
        }

        unsigned int deviceCount = 0;
        cudaError_t err;

        err = cudaGLGetDevices(&deviceCount, nullptr, 0, cudaGLDeviceListAll);
        if (err != cudaSuccess || deviceCount == 0) {
            qDebug() << "获取 OpenGL 关联的 CUDA 设备失败:" << cudaGetErrorString(err);
            return false;
        }

        // 2. 分配内存存储设备ID列表
        std::vector<int> deviceIds(deviceCount);
        err = cudaGLGetDevices(&deviceCount, deviceIds.data(), deviceCount, cudaGLDeviceListAll);
        if (err != cudaSuccess) {
            qDebug() << "获取设备ID列表失败:" << cudaGetErrorString(err);
            return false;
        }

        // 3. 选择第一个可用设备（通常只有一个）
        targetDevice = deviceIds[0];
        // qDebug() << "与 OpenGL 关联的 CUDA 设备ID:" << targetDevice;

        // 4. 设置当前设备（替代 cudaGLSetGLDevice）
        err = cudaSetDevice(targetDevice);
        if (err != cudaSuccess) {
            qDebug() << "设置 CUDA 设备失败:" << cudaGetErrorString(err);
            return false;
        }

        this->fboTex = fboTexId;
        this->fboWidth = width;
        this->fboHeight = height;

        err = cudaGraphicsGLRegisterImage(
            &fboCudaResource,
            fboTex,
            GL_TEXTURE_2D,
            cudaGraphicsRegisterFlagsReadOnly  // 因为你是用于编码/读取
            );
        if (err != cudaSuccess) {
            qWarning() << "[FBO] cudaGraphicsGLRegisterImage failed:" << cudaGetErrorString(err);
            return false;
        }

        return true;
    }

    // 映射FBO
    cudaArray_t mapFboCudaArray()
    {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        if (!fboCudaResource) {
            qWarning() << "fboCudaResource is nullptr";
            return nullptr;
        }

        cudaError_t err = cudaGraphicsMapResources(1, &fboCudaResource, 0);
        if (err != cudaSuccess) {
            qWarning() << "cudaGraphicsMapResources (FBO) failed:" << cudaGetErrorString(err);
            return nullptr;
        }

        cudaArray_t fboArray = nullptr;
        err = cudaGraphicsSubResourceGetMappedArray(&fboArray, fboCudaResource, 0, 0);
        if (err != cudaSuccess || !fboArray) {
            qWarning() << "cudaGraphicsSubResourceGetMappedArray (FBO) failed:" << cudaGetErrorString(err);
            cudaGraphicsUnmapResources(1, &fboCudaResource, 0);
            return nullptr;
        }

        return fboArray;  // 映射后，外部需要记得 unmap！
    }

    void unmapFboCudaArray()
    {
        if (fboCudaResource) {
            cudaGraphicsUnmapResources(1, &fboCudaResource, 0);
        }
    }

    GLuint texture() const {
        return glTex;
    }

    int getWidth() const{
        return width;
    }

    int getHeight() const{
        return height;
    }
    bool isInitialized() const {
        return initialized;
    }

    void release() {
        std::lock_guard<std::mutex> lock(resourceMutex_);
        waitForOperationsToComplete();
        if (cudaResource) {
            cudaGraphicsUnregisterResource(cudaResource);
            cudaResource = nullptr;
        }

        if (fboCudaResource) {
            cudaGraphicsUnregisterResource(fboCudaResource);
            fboCudaResource = nullptr;
        }
        if (QOpenGLContext::currentContext()) {
            QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
            if (glTex) {
                f->glDeleteTextures(1, &glTex);
                glTex = 0;
            }
        }
        if (rgbaBuffer) {
            cudaFree(rgbaBuffer);
            rgbaBuffer = nullptr;
        }

        if (bgraBuffer) {
            cudaFree(bgraBuffer);
            bgraBuffer = nullptr;
        }

        pitchRGBA = 0;
        pitchBGRA = 0;

        initialized = false;
        width = 0;
        height = 0;
        cachedW = 0;
        cachedH = 0;
    }

};
#endif // CUDAINTEROPHELPERIMPL_H
