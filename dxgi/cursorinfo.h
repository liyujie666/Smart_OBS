// cursorinfo.h
#ifndef CURSOR_INFO_H
#define CURSOR_INFO_H

#include <windows.h>
#include <dxgi1_2.h>

// 鼠标形状信息（与DXGI兼容，但不依赖CUDA）
struct CursorShapeInfo {
    BYTE* buffer = nullptr;          // 鼠标形状数据缓冲区（CPU内存）
    DXGI_OUTDUPL_POINTER_SHAPE_INFO dxgiShape = {}; // DXGI形状信息
    UINT bufferSize = 0;             // 缓冲区大小
};

// 完整鼠标信息（位置、可见性、形状等）
struct CursorInfo {
    POINT position = {};             // 鼠标位置（桌面坐标系）
    bool visible = false;            // 是否可见
    CursorShapeInfo shapeInfo;       // 形状信息
    UINT outputIndex = 0;            // 输出设备索引
    LARGE_INTEGER preTimestamp = {};
};

#endif // CURSOR_INFO_H
