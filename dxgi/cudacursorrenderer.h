// #ifndef CUDA_CURSOR_RENDERER_H
// #define CUDA_CURSOR_RENDERER_H

// #include <cuda_runtime.h>
// #include "cursorinfo.h"  // 依赖通用鼠标信息结构体
// #include <atomic>


// // CUDA鼠标渲染器（仅负责GPU绘制，不依赖DXGI）
// class CudaCursorRenderer {
// public:
//     CudaCursorRenderer() = default;
//     ~CudaCursorRenderer() = default;

//     // 绘制鼠标到桌面CUDA数组（核心接口）
//     bool renderCursor(
//         cudaArray_t desktopArray,  // 桌面图像的CUDA数组
//         int desktopWidth,          // 桌面宽度
//         int desktopHeight,         // 桌面高度
//         const CursorInfo& cursorInfo  // 鼠标信息（来自DxgiDesktopDuplicator）
//         );

//     // 设置是否绘制鼠标（线程安全）
//     void setDrawMouse(bool enable) { drawMouse_.store(enable); }
//     bool isDrawMouse() const { return drawMouse_.load(); }

// private:
//     std::atomic<bool> drawMouse_ = false;  // 绘制开关
// };

// #endif // CUDA_CURSOR_RENDERER_H

#ifndef CUDA_CURSOR_RENDERER_H
#define CUDA_CURSOR_RENDERER_H

#include <cuda_runtime.h>
#include "cursorinfo.h"

#ifdef __cplusplus
extern "C" {
#endif

// 绘制鼠标到桌面CUDA数组
bool cudaRenderCursor(
    cudaArray_t desktopArray,   // 桌面图像的CUDA数组
    int desktopWidth,           // 桌面宽度
    int desktopHeight,          // 桌面高度
    const CursorInfo* cursorInfo, // 鼠标信息（指针形式）
    bool drawMouse,              // 是否绘制鼠标
    const RECT& displayRect,
    DXGI_MODE_ROTATION rotation
    );

#ifdef __cplusplus
}
#endif

#endif // CUDA_CURSOR_RENDERER_H
