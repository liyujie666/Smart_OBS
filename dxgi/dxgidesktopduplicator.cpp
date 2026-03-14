#include "dxgidesktopduplicator.h"
#include <QDebug>

using Microsoft::WRL::ComPtr;
const int ROTATE270_X_OFFSET = 35;
DxgiDesktopDuplicator::DxgiDesktopDuplicator()
    :cursorBuffer_(nullptr)
{

}
DxgiDesktopDuplicator::~DxgiDesktopDuplicator()
{
    std::lock_guard<std::mutex> lock(cursorMutex_);
    // if (cursorInfo_.shapeInfo.buffer) {
    //     delete[] cursorInfo_.shapeInfo.buffer;
    //     cursorInfo_.shapeInfo.buffer = nullptr;
    // }
    shutdown();
}

bool DxgiDesktopDuplicator::initialize(int adapterIndex, int outputIndex)
{
    HRESULT hr;

    // 1. 创建DXGI工厂
    ComPtr<IDXGIFactory1> factory;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(factory.GetAddressOf()));
    if (FAILED(hr)) {
        qWarning("CreateDXGIFactory1 failed: 0x%X", hr);
        return false;
    }

    // 2. 获取显卡适配器
    hr = factory->EnumAdapters1(adapterIndex, &adapter_);
    if (FAILED(hr)) {
        qWarning("EnumAdapters1 failed for index %d: 0x%X", adapterIndex, hr);
        return false;
    }

    // 3. 创建D3D11设备
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        adapter_.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &d3dDevice_, &featureLevel, &d3dContext_);
    if (FAILED(hr)) {
        qWarning("D3D11CreateDevice failed: 0x%X", hr);
        return false;
    }

    if (featureLevel < D3D_FEATURE_LEVEL_11_0) {
        qWarning("设备不支持Direct3D 11，无法使用ps_5_0/vs_5_0");
        // 降级着色器版本为ps_4_0_level_9_1（兼容DX10/10.1设备）
        // 需同步修改D3DCompile的目标版本（如"ps_4_0_level_9_1"）
        return false;
    }

    // 4. 获取输出设备（显示器）
    ComPtr<IDXGIOutput> output;
    hr = adapter_->EnumOutputs(outputIndex, &output);
    if (FAILED(hr)) {
        qWarning("EnumOutputs failed for index %d: 0x%X", outputIndex, hr);
        return false;
    }

    hr = output->GetDesc(&outputDesc_);  // 保存到成员变量outputDesc_
    if (FAILED(hr)) {
        qWarning("获取输出设备描述失败: 0x%X", hr);
        return false;
    }

    // 转换为IDXGIOutput1接口
    hr = output.As(&output1_);
    if (FAILED(hr)) {
        qWarning("Failed to convert to IDXGIOutput1: 0x%X", hr);
        return false;
    }

    // 5. 创建桌面复制器
    hr = output1_->DuplicateOutput(d3dDevice_.Get(), &duplication_);
    if (FAILED(hr)) {
        qWarning("DuplicateOutput failed: 0x%X", hr);
        return false;
    }

    // 6. 获取桌面复制器描述（逻辑尺寸）
    DXGI_OUTDUPL_DESC desc;
    duplication_->GetDesc(&desc);

    // 7. 获取显示器旋转状态
    DXGI_OUTPUT_DESC outputDesc;
    hr = output1_->GetDesc(&outputDesc);
    if (FAILED(hr)) {
        qWarning("GetOutputDesc failed: 0x%X", hr);
        return false;
    }
    logicalWidth_ = desc.ModeDesc.Width;
    logicalHeight_ = desc.ModeDesc.Height;

    // 获取当前显示模式的旋转信息
    rotation_ = outputDesc.Rotation;

    qDebug() << "显示器旋转状态: " << rotation_
             << " (1=正常, 2=90度, 3=180度, 4=270度)";

    // 8. 根据旋转状态计算物理纹理尺寸
    // 竖屏旋转(90/270度)时，物理尺寸是逻辑尺寸的宽高颠倒
    if (rotation_ == DXGI_MODE_ROTATION_ROTATE90 ||
        rotation_ == DXGI_MODE_ROTATION_ROTATE270) {
        width_ = desc.ModeDesc.Height;   // 物理宽度 = 逻辑高度
        height_ = desc.ModeDesc.Width;   // 物理高度 = 逻辑宽度
    } else {
        width_ = desc.ModeDesc.Width;    // 物理宽度 = 逻辑宽度
        height_ = desc.ModeDesc.Height;  // 物理高度 = 逻辑高度
    }
    qDebug() << "物理纹理尺寸: " << width_ << "x" << height_
             << "逻辑尺寸：" << logicalWidth_ << "x" << logicalHeight_;

    // 9. 创建匹配物理尺寸的共享纹理
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width_;
    texDesc.Height = height_;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // 与桌面纹理格式一致
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;  // 支持共享给CUDA

    hr = d3dDevice_->CreateTexture2D(&texDesc, nullptr, &sharedTex_);
    if (FAILED(hr)) {
        qWarning("Failed to create shared texture: 0x%X", hr);
        return false;
    }

    // 10. 注册CUDA资源
    cudaError_t cudaErr = cudaGraphicsD3D11RegisterResource(
        &cudaResource_, sharedTex_.Get(),
        cudaGraphicsRegisterFlagsNone
        );
    if (cudaErr != cudaSuccess) {
        qWarning("cudaGraphicsD3D11RegisterResource failed: %s",
                 cudaGetErrorString(cudaErr));
        return false;
    }

    qDebug() << "DXGI初始化成功 - 物理尺寸:" << width_ << "x" << height_;


    return true;
}

bool DxgiDesktopDuplicator::acquireFrame()
{
    if (!duplication_) return false;

    // 1. 获取桌面帧
    HRESULT hr = duplication_->AcquireNextFrame(10, &frameInfo_, &desktopResource_);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            return false;
        }
        // qWarning("AcquireNextFrame failed: 0x%X", hr);
        return false;
    }
    // 2. 更新鼠标信息
    updateCursorInfo(frameInfo_);

    // 3. 处理桌面纹理
    ComPtr<ID3D11Texture2D> tex;
    hr = desktopResource_.As(&tex);
    if (FAILED(hr)) {
        releaseFrame();  // 出错时释放帧，重置状态
        return false;
    }

    d3dContext_->CopyResource(sharedTex_.Get(), tex.Get());

    // 标记帧已获取（此时 desktopResource_ 已有效）
    frameAcquired_ = true;

    return true;
}


void DxgiDesktopDuplicator::releaseFrame()
{
    if (duplication_ && frameAcquired_) {
        duplication_->ReleaseFrame();
        desktopResource_.Reset();
        frameAcquired_ = false;
    }
}

cudaArray_t DxgiDesktopDuplicator::getCudaArray()
{
    std::lock_guard<std::mutex> lock(cudaResourceMutex);
    if (!cudaResource_) return nullptr;

    cudaGraphicsMapResources(1, &cudaResource_, 0);

    cudaArray_t array;
    cudaGraphicsSubResourceGetMappedArray(&array, cudaResource_, 0, 0);
    return array;
}

void DxgiDesktopDuplicator::unmapCudaArray()
{
    std::lock_guard<std::mutex> lock(cudaResourceMutex);
    if (cudaResource_) cudaGraphicsUnmapResources(1, &cudaResource_, 0);
}

void DxgiDesktopDuplicator::shutdown()
{
    releaseFrame();

    {
        std::lock_guard<std::mutex> lock(cudaResourceMutex); // 加锁释放
        if (cudaResource_) {
            cudaGraphicsUnmapResources(1, &cudaResource_, 0);
            cudaGraphicsUnregisterResource(cudaResource_);
            cudaResource_ = nullptr;
        }
    }
    sharedTex_.Reset();
    duplication_.Reset();
    output1_.Reset();
    adapter_.Reset();
    d3dContext_.Reset();
    d3dDevice_.Reset();
}

void DxgiDesktopDuplicator::updateCursorInfo(const DXGI_OUTDUPL_FRAME_INFO& frameInfo) {
    std::lock_guard<std::mutex> lock(cursorMutex_);

    // 无鼠标更新时直接返回（时间戳为0表示无更新）
    if (frameInfo.LastMouseUpdateTime.QuadPart == 0) {
        return;
    }

    bool b_updated = true;

    // 1. 获取鼠标事件所属的输出设备索引（DXGI隐藏API，需通过位置判断）
    RECT currentDisplayRect = outputDesc_.DesktopCoordinates;
    bool isCursorOnCurrentDisplay = (
        frameInfo.PointerPosition.Position.x >= 0 &&
        frameInfo.PointerPosition.Position.y >= 0 &&
        frameInfo.PointerPosition.Position.x < (currentDisplayRect.right - currentDisplayRect.left) &&
        frameInfo.PointerPosition.Position.y < (currentDisplayRect.bottom - currentDisplayRect.top)
        );

    // 2. 只有鼠标在当前显示器上，才更新信息
    if (!isCursorOnCurrentDisplay) {
        b_updated = false;
    }

    // 3. 时间戳过滤：只接受更新的事件（避免旧事件覆盖新事件）
    if (frameInfo.LastMouseUpdateTime.QuadPart <= cursorInfo_.preTimestamp.QuadPart) {
        b_updated = false;
    }

    // 更新鼠标基础信息
    if (b_updated) {
        // 1. 计算原始鼠标位置（相对于当前显示器的局部坐标）
        int logicalX = frameInfo.PointerPosition.Position.x;
        int logicalY = frameInfo.PointerPosition.Position.y;

        // 2. 根据旋转状态转换为物理纹理坐标（关键适配）
        int physicalX = logicalX;
        int physicalY = logicalY;

        // qDebug() << "before physicalX : " << physicalX
        //          << "physicalY : " << physicalY;
        switch (rotation_) {
        case DXGI_MODE_ROTATION_ROTATE90:
            physicalX = logicalY;
            physicalY = logicalWidth_ - logicalX - 1;
            break;
        case DXGI_MODE_ROTATION_ROTATE270:
            physicalX = logicalHeight_ - logicalY - ROTATE270_X_OFFSET;
            physicalY = logicalX;
            break;
        case DXGI_MODE_ROTATION_ROTATE180:
            physicalX = logicalWidth_ - logicalX - 1;
            physicalY = logicalHeight_ - logicalY - 1;
            break;
        default:
            physicalX = logicalX;
            physicalY = logicalY;
            break;
        }
        // qDebug() << "after physicalX : " << physicalX
        //          << "physicalY : " << physicalY;

        // 2. 转换为全局桌面坐标（加上当前显示器的桌面偏移）
        RECT currentDisplayRect = outputDesc_.DesktopCoordinates;
        int desktopLeft = currentDisplayRect.left;
        int desktopTop = currentDisplayRect.top;
        cursorInfo_.position.x = desktopLeft + physicalX;
        cursorInfo_.position.y = desktopTop + physicalY;

        // 3. 范围校验（限制在当前显示器范围内）
        LONG maxX = static_cast<LONG>(desktopLeft + width_ - 1);
        LONG maxY = static_cast<LONG>(desktopTop + height_ - 1);
        cursorInfo_.position.x = std::clamp(cursorInfo_.position.x, static_cast<LONG>(desktopLeft), maxX);
        cursorInfo_.position.y = std::clamp(cursorInfo_.position.y, static_cast<LONG>(desktopTop), maxY);

        // 4. 其他信息更新
        cursorInfo_.outputIndex = outputIndex_;
        cursorInfo_.preTimestamp = frameInfo.LastMouseUpdateTime;
        cursorInfo_.visible = (frameInfo.PointerPosition.Visible != 0);
    }


    // 3. 处理鼠标形状更新（仅当有形状数据时）
    if (frameInfo.PointerShapeBufferSize == 0) {
        // 无形状更新，仅保留位置等基础信息更新
        return;
    }

    cursorBuffer_.reset(new BYTE[frameInfo.PointerShapeBufferSize]);
    cursorInfo_.shapeInfo.buffer = cursorBuffer_.get();
    cursorInfo_.shapeInfo.bufferSize = frameInfo.PointerShapeBufferSize;

    // 5. 获取鼠标形状数据（通过DXGI接口）
    UINT bufferSizeRequired = 0;
    HRESULT hr = duplication_->GetFramePointerShape(
        frameInfo.PointerShapeBufferSize,          // 输入：缓冲区大小
        cursorInfo_.shapeInfo.buffer,              // 输出：形状数据
        &bufferSizeRequired,                       // 输出：实际需要的大小
        &cursorInfo_.shapeInfo.dxgiShape           // 输出：形状信息（宽/高/类型等）
        );

    // 处理获取失败的情况
    if (FAILED(hr) || bufferSizeRequired > frameInfo.PointerShapeBufferSize) {
        cursorBuffer_.reset();
        cursorInfo_.shapeInfo.buffer = nullptr;
        cursorInfo_.shapeInfo.bufferSize = 0;
        ZeroMemory(&cursorInfo_.shapeInfo.dxgiShape, sizeof(DXGI_OUTDUPL_POINTER_SHAPE_INFO));
    }
}


bool DxgiDesktopDuplicator::getCursorInfo(CursorInfo& outInfo) const {
    std::lock_guard<std::mutex> lock(cursorMutex_);

    outInfo.position = cursorInfo_.position;
    outInfo.visible = cursorInfo_.visible;
    outInfo.outputIndex = cursorInfo_.outputIndex;

    if (cursorInfo_.shapeInfo.buffer && cursorInfo_.shapeInfo.bufferSize > 0) {
        outInfo.shapeInfo.bufferSize = cursorInfo_.shapeInfo.bufferSize;
        outInfo.shapeInfo.buffer = new BYTE[outInfo.shapeInfo.bufferSize];
        memcpy(outInfo.shapeInfo.buffer, cursorInfo_.shapeInfo.buffer, outInfo.shapeInfo.bufferSize);
        outInfo.shapeInfo.dxgiShape = cursorInfo_.shapeInfo.dxgiShape;
    } else {
        outInfo.shapeInfo.buffer = nullptr;
        outInfo.shapeInfo.bufferSize = 0;
    }
    return true;
}
