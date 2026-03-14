// #ifndef WGCCAPTURECORE_H
// #define WGCCAPTURECORE_H

// #include <unknwn.h>
// #include <windows.h>
// #include <QObject>
// #include <QThread>
// #include <atomic>
// #include <d3d11.h>
// #include <wrl/client.h>

// // 2. CUDA头文件（必须在WinRT前，解决cuda函数未声明）
// #include <cuda_runtime.h>
// #include <cuda_d3d11_interop.h>

// // 3. 禁用WinRT协程（彻底解决co_await错误）
// #define WINRT_NO_COROUTINES
// #define WINRT_LEAN_AND_MEAN
// #include <winrt/base.h>
// #include <winrt/Windows.Graphics.Capture.h>
// #include <winrt/Windows.Graphics.DirectX.h>
// #include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
// #include <winrt/Windows.UI.h>
// #include <windows.graphics.capture.interop.h>  // 关键：用于CreateForWindow

// // 简化命名空间
// using namespace winrt;
// using namespace winrt::Windows::Graphics::Capture;
// using namespace winrt::Windows::Graphics::DirectX;
// using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
// using namespace Microsoft::WRL;

// struct WgcConfig {
//     HWND targetHwnd = nullptr;      // 目标窗口句柄
//     bool enableDrawMouse = false;   // 是否捕捉鼠标
//     int frameRate = 30;             // 目标帧率
// };

// struct WgcRawFrame {
//     int64_t timestamp;               // 时间戳（微秒）
//     cudaArray_t bgraArray;           // Cuda数组（BGRA格式）
//     int width;                       // 帧宽
//     int height;                      // 帧高
// };

// class WgcCaptureCore : public QObject {
//     Q_OBJECT
// public:
//     explicit WgcCaptureCore(QObject* parent = nullptr);
//     ~WgcCaptureCore() override;

//     int initialize(const WgcConfig& config);
//     bool startCapture();
//     void stopCapture();
//     bool isCapture() const { return m_isCapturing.load(); }

// signals:
//     void rawFrameReady(const WgcRawFrame& rawFrame);

// private:
//     // 辅助函数：初始化D3D11设备
//     bool initD3D11Device();
//     // 辅助函数：将WRL的ID3D11Device转换为WinRT的IDirect3DDevice
//     IDirect3DDevice getWinRTDevice() const;
//     // 辅助函数：D3D纹理注册为Cuda资源
//     bool registerD3DTexToCuda(ID3D11Texture2D* d3dTex, cudaGraphicsResource_t& cudaRes);
//     // 辅助函数：从Cuda资源获取cudaArray_t
//     bool getCudaArray(cudaGraphicsResource_t cudaRes, cudaArray_t& cuArray);

// private:
//     WgcConfig m_config;
//     std::atomic<bool> m_isCapturing = false;
//     std::atomic<bool> m_isCudaInited = false;

//     // D3D相关（WRL智能指针）
//     ComPtr<ID3D11Device> m_d3dDevice;
//     ComPtr<ID3D11DeviceContext> m_d3dContext;

//     // WGC核心对象（C++/WinRT对象）
//     GraphicsCaptureItem m_wgcItem{nullptr};
//     Direct3D11CaptureFramePool m_framePool{nullptr};
//     GraphicsCaptureSession m_wgcSession{nullptr};
//     event_token m_frameArrivedToken{};  // 帧事件令牌

//     // Cuda互操作
//     cudaGraphicsResource_t m_cudaResource = nullptr;
// };

// #endif // WGCCAPTURECORE_H
