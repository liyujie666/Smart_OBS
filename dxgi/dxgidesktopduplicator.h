#ifndef DXGIDESKTOPDUPLICATOR_H
#define DXGIDESKTOPDUPLICATOR_H
#include <windows.h>
#include "cuda.h"
#include "cursorinfo.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cuda_runtime.h>
#include <cuda_d3d11_interop.h>
#include <vector>
#include <mutex>
#include <atomic>

using Microsoft::WRL::ComPtr;



class DxgiDesktopDuplicator
{
public:
    DxgiDesktopDuplicator();
    ~DxgiDesktopDuplicator();

    bool initialize(int adapterIndex = 0, int outputIndex = 0);
    bool acquireFrame();
    void releaseFrame();
    void shutdown();

    cudaArray_t getCudaArray();
    bool getCursorInfo(CursorInfo& outInfo) const;
    ID3D11Texture2D* getSharedTexture() const { return sharedTex_.Get(); }
    void unmapCudaArray();

    ID3D11Device* device() const { return d3dDevice_.Get(); }
    int width() const { return width_; }
    int height() const { return height_; }

    int physicalWidth() const { return physicalWidth_; }
    int physicalHeight() const { return physicalHeight_; }
    int logicalWidth() const { return logicalWidth_; }
    int logicalHeight() const { return logicalHeight_; }

    DXGI_MODE_ROTATION rotation() const { return rotation_; }
    RECT getDisplayRect() const { return outputDesc_.DesktopCoordinates; }


private:
    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<IDXGIOutputDuplication> duplication_;
    ComPtr<IDXGIAdapter1> adapter_;
    ComPtr<IDXGIOutput1> output1_;
    ComPtr<ID3D11Texture2D> sharedTex_;
    ComPtr<IDXGIResource> desktopResource_;
    DXGI_OUTDUPL_FRAME_INFO frameInfo_;
    DXGI_MODE_ROTATION rotation_;
    bool frameAcquired_ = false;
    int width_;
    int height_;

    cudaGraphicsResource* cudaResource_ = nullptr;
    std::mutex cudaResourceMutex;
    std::atomic<bool> isForcedStop{false};

    int physicalWidth_;   // 新增：物理宽度（旋转前）
    int physicalHeight_;  // 新增：物理高度（旋转前）

    int logicalWidth_ = 1920;
    int logicalHeight_ = 1080;

    mutable std::mutex cursorMutex_;  // 保护鼠标数据的互斥锁
    CursorInfo cursorInfo_;           // 内部鼠标信息
    int outputIndex_ = 0;             // 输出设备索引
    DXGI_OUTPUT_DESC outputDesc_;     // 输出设备描述
    std::unique_ptr<BYTE[]> cursorBuffer_;

    // 更新鼠标信息
    void updateCursorInfo(const DXGI_OUTDUPL_FRAME_INFO& frameInfo);

};
#endif // DXGIDESKTOPDUPLICATOR_H

