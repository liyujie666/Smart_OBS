// #include "wgccapturecore.h"
// #include <QDebug>
// #include <windows.h>
// #include <objbase.h>
// #include <chrono>
// #include <dxgi1_2.h>

// // 构造函数：初始化COM和Cuda
// WgcCaptureCore::WgcCaptureCore(QObject* parent) : QObject(parent) {
//     // 初始化COM（单线程公寓）
//     HRESULT comRet = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
//     if (FAILED(comRet) && comRet != RPC_E_CHANGED_MODE) {
//         qCritical() << "WgcCaptureCore：COM初始化失败，错误码：" << comRet;
//     }

//     // 初始化Cuda
//     cudaError_t cudaErr = cudaSetDevice(0);
//     if (cudaErr != cudaSuccess) {
//         qCritical() << "WgcCaptureCore：Cuda初始化失败，错误：" << cudaGetErrorString(cudaErr);
//         m_isCudaInited = false;
//     } else {
//         m_isCudaInited = true;
//     }
// }

// // 析构函数：释放资源
// WgcCaptureCore::~WgcCaptureCore() {
//     stopCapture();

//     // 释放Cuda资源
//     if (m_cudaResource != nullptr) {
//         cudaGraphicsUnregisterResource(m_cudaResource);
//         m_cudaResource = nullptr;
//     }

//     // 释放COM
//     CoUninitialize();
// }

// // 将WRL的ID3D11Device转换为WinRT的IDirect3DDevice（修正转换错误）
// IDirect3DDevice WgcCaptureCore::getWinRTDevice() const {
//     ComPtr<IDXGIDevice> dxgiDevice;
//     HRESULT hr = m_d3dDevice.As(&dxgiDevice);
//     if (FAILED(hr)) {
//         qCritical() << "WgcCaptureCore：获取DXGI设备失败，错误码：" << hr;
//         return nullptr;
//     }

//     // 使用WinRT API创建IDirect3DDevice
//     auto dxgiDevicePtr = dxgiDevice.Get();
//     return CreateDirect3D11DeviceFromDXGIDevice(dxgiDevicePtr);
// }

// // 初始化WGC捕捉（修正所有接口错误）
// int WgcCaptureCore::initialize(const WgcConfig& config) {
//     // 前置检查
//     if (!m_isCudaInited) return -1;
//     if (config.targetHwnd == nullptr) {
//         qCritical() << "WgcCaptureCore：目标HWND为空";
//         return -2;
//     }
//     if (!initD3D11Device()) return -3;

//     m_config = config;

//     // 目标窗口设置：最小化仍可捕捉
//     if (!SetWindowDisplayAffinity(m_config.targetHwnd, WDA_MONITOR)) {
//         qWarning() << "WgcCaptureCore：SetWindowDisplayAffinity失败，最小化可能黑屏";
//     }

//     try {
//         // 创建GraphicsCaptureItem（修正CreateForWindow错误，使用正确的命名空间）
//         m_wgcItem = GraphicsCaptureItem::CreateForWindow(m_config.targetHwnd);

//         // 创建帧池（修正DirectXPixelFormat未声明错误）
//         m_framePool = Direct3D11CaptureFramePool::Create(
//             getWinRTDevice(),
//             DirectXPixelFormat::B8G8R8A8UIntNormalized,  // 完整命名空间引用
//             1,  // 缓冲区数量
//             m_wgcItem.Size()  // 初始尺寸
//             );

//         // 创建捕捉会话
//         m_wgcSession = m_framePool.CreateCaptureSession(m_wgcItem);

//         // 配置鼠标捕捉
//         m_wgcSession.IsCursorCaptureEnabled(m_config.enableDrawMouse);

//         // 注册帧到达事件（修正lambda回调）
//         m_frameArrivedToken = m_framePool.FrameArrived([this](const auto& sender, const auto&) {
//             if (!m_isCapturing) return;

//             // 获取下一帧
//             auto frame = sender.TryGetNextFrame();
//             if (!frame) return;

//             // 获取D3D表面
//             IDirect3DSurface surface = frame.Surface();

//             // 转换为ID3D11Texture2D（修正as()方法错误）
//             ComPtr<ID3D11Texture2D> d3dTex;
//             HRESULT hr = reinterpret_cast<IUnknown*>(surface)->QueryInterface(IID_PPV_ARGS(&d3dTex));
//             if (FAILED(hr)) {
//                 qWarning() << "WgcCaptureCore：D3D表面转纹理失败，错误码：" << hr;
//                 return;
//             }

//             // 获取纹理参数
//             D3D11_TEXTURE2D_DESC texDesc;
//             d3dTex->GetDesc(&texDesc);
//             int width = texDesc.Width;
//             int height = texDesc.Height;
//             if (width <= 0 || height <= 0) return;

//             // D3D纹理转Cuda数组
//             if (!registerD3DTexToCuda(d3dTex.Get(), m_cudaResource)) return;
//             cudaArray_t cuArray = nullptr;
//             if (!getCudaArray(m_cudaResource, cuArray)) {
//                 cudaGraphicsUnmapResources(1, &m_cudaResource, nullptr);
//                 return;
//             }

//             // 构造帧数据并发送信号
//             WgcRawFrame rawFrame;
//             rawFrame.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
//                                      std::chrono::system_clock::now().time_since_epoch()
//                                      ).count();
//             rawFrame.bgraArray = cuArray;
//             rawFrame.width = width;
//             rawFrame.height = height;
//             emit rawFrameReady(rawFrame);

//             // 解除Cuda资源映射
//             cudaGraphicsUnmapResources(1, &m_cudaResource, nullptr);

//             // 帧率控制
//             if (m_config.frameRate > 0) {
//                 QThread::usleep(1000000 / m_config.frameRate);
//             }
//         });

//         qInfo() << "WgcCaptureCore：初始化成功，目标HWND：" << (void*)m_config.targetHwnd;
//         return 0;
//     }
//     catch (const hresult_error& e) {
//         qCritical() << "WgcCaptureCore：初始化失败，错误码：" << e.code()
//                     << "，消息：" << QString::fromWCharArray(e.message().c_str());
//         return -4;
//     }
// }

// // 启动捕捉
// bool WgcCaptureCore::startCapture() {
//     if (m_isCapturing || !m_wgcSession) return false;

//     try {
//         m_wgcSession.StartCapture();
//         m_isCapturing = true;
//         qInfo() << "WgcCaptureCore：捕捉已启动";
//         return true;
//     }
//     catch (const hresult_error& e) {
//         qCritical() << "WgcCaptureCore：启动捕捉失败，错误码：" << e.code();
//         return false;
//     }
// }

// // 停止捕捉
// void WgcCaptureCore::stopCapture() {
//     if (!m_isCapturing) return;

//     // 停止会话
//     if (m_wgcSession) {
//         m_wgcSession.Close();
//     }

//     // 移除事件注册
//     if (m_frameArrivedToken) {
//         m_framePool.FrameArrived(m_frameArrivedToken);
//         m_frameArrivedToken = {};
//     }

//     // 重置WGC对象
//     m_wgcSession = nullptr;
//     m_framePool = nullptr;
//     m_wgcItem = nullptr;

//     m_isCapturing = false;
//     qInfo() << "WgcCaptureCore：捕捉已停止";
// }

// // 初始化D3D11设备
// bool WgcCaptureCore::initD3D11Device() {
//     if (m_d3dDevice) return true;

//     D3D_FEATURE_LEVEL featureLevel;
//     const D3D_FEATURE_LEVEL featureLevels[] = {
//         D3D_FEATURE_LEVEL_11_0,
//         D3D_FEATURE_LEVEL_10_1
//     };

//     HRESULT hr = D3D11CreateDevice(
//         nullptr,                      // 默认适配器
//         D3D_DRIVER_TYPE_HARDWARE,     // 硬件加速
//         nullptr,                      // 无软件驱动
//         D3D11_CREATE_DEVICE_BGRA_SUPPORT |  // 支持BGRA格式
//             D3D11_CREATE_DEVICE_DEBUG,    // 调试模式（发布时移除）
//         featureLevels,
//         ARRAYSIZE(featureLevels),
//         D3D11_SDK_VERSION,
//         &m_d3dDevice,
//         &featureLevel,
//         &m_d3dContext
//         );

//     if (FAILED(hr)) {
//         qCritical() << "WgcCaptureCore：D3D11设备创建失败，错误码：" << hr;
//         return false;
//     }

//     qInfo() << "WgcCaptureCore：D3D11设备初始化成功，FeatureLevel：" << featureLevel;
//     return true;
// }

// // D3D纹理注册为Cuda资源
// bool WgcCaptureCore::registerD3DTexToCuda(ID3D11Texture2D* d3dTex, cudaGraphicsResource_t& cudaRes) {
//     if (!d3dTex || !m_isCudaInited) return false;

//     // 释放旧资源
//     if (cudaRes != nullptr) {
//         cudaGraphicsUnregisterResource(cudaRes);
//         cudaRes = nullptr;
//     }

//     // 注册为Cuda只读资源
//     cudaError_t cudaErr = cudaGraphicsD3D11RegisterResource(
//         &cudaRes, d3dTex, cudaGraphicsRegisterFlagsReadOnly
//         );
//     if (cudaErr != cudaSuccess) {
//         qWarning() << "WgcCaptureCore：Cuda注册D3D纹理失败，错误：" << cudaGetErrorString(cudaErr);
//         return false;
//     }
//     return true;
// }

// // 从Cuda资源获取cudaArray_t（解决未声明错误）
// bool WgcCaptureCore::getCudaArray(cudaGraphicsResource_t cudaRes, cudaArray_t& cuArray) {
//     if (!cudaRes || !m_isCudaInited) return false;

//     // 映射Cuda资源
//     cudaError_t cudaErr = cudaGraphicsMapResources(1, &cudaRes, nullptr);
//     if (cudaErr != cudaSuccess) {
//         qWarning() << "WgcCaptureCore：Cuda映射资源失败，错误：" << cudaGetErrorString(cudaErr);
//         return false;
//     }

//     // 提取cudaArray_t（确保包含cuda_d3d11_interop.h）
//     cudaErr = cudaGraphicsSubResourceGetMappedArray(&cuArray, cudaRes, 0,0);
//     if (cudaErr != cudaSuccess) {
//         qWarning() << "WgcCaptureCore：Cuda获取数组失败，错误：" << cudaGetErrorString(cudaErr);
//         cudaGraphicsUnmapResources(1, &cudaRes, nullptr);
//         return false;
//     }
//     return true;
// }
