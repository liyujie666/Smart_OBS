# GPU 零拷贝合成管线 —— 面试准备文档

## 一、架构总览

```
┌─────────────┐    ┌──────────────────┐    ┌───────────────────┐    ┌──────────────┐
│ DXGI 采集    │    │  CUDA Kernel     │    │ OpenGL FBO 合成  │    │ NVENC 编码    │
│ (D3D11纹理)  │──▶│  格式转换    │──▶│  多图层离屏渲染    │──▶│  (读FBO纹理)  │
└─────────────┘    └──────────────────┘    └───────────────────┘    └──────────────┘
     cudaArray    线性GPU内存   GL Texture           cudaArray
   (BGRA)    (RGBA)         (RGBA)   (RGBA→NV12)
```

**核心思想**：全程数据留存 GPU 显存，通过 CUDA-D3D11 互操作和 CUDA-OpenGL 互操作实现零拷贝跨 API 访问。

---

## 二、关键数据结构

### CudaFrameInfo（帧数据描述符）
```cpp
struct CudaFrameInfo {
    FrameFormat format;        // NV12 或 BGRA
    VideoSourceType type;      // Camera/Desktop/Media/Text
    union {
      struct { CUdeviceptr yPlane; CUdeviceptr uvPlane; };  // NV12（摄像头/硬解）
     cudaArray_t bgraArray;     // BGRA（桌面采集）
    };
    int pitchY, pitchUV, width, height;
    int sourceId, sceneId, priority;
    int64_t timestamp, pts;
};
```

### FrameLayer（图层）
```cpp
class FrameLayer {
    CudaFrameInfo frameInfo;
    CudaInteropHelper* interopHelper;  // 每个图层独立的互操作器
    QRectF rect; // 像素坐标
    QRectF rectRatio; // 归一化坐标
    float selfAspectRatio;
    int selfRotateType;          // 0=正常, 1=90°, 2=270°
    bool isVisible, isActive, isLocked, isSelected;
};
```

### CudaInteropHelperImpl（互操作核心）
```cpp
class CudaInteropHelperImpl {
    GLuint glTex;      // OpenGL 纹理
    cudaGraphicsResource* cudaResource;    // GL纹理注册的CUDA资源（写入方向）
    cudaGraphicsResource* fboCudaResource; // FBO纹理注册的CUDA资源（读取方向）
    uint8_t* rgbaBuffer;  // GPU 线性 RGBA 缓冲
    uint8_t* bgraBuffer;       // GPU 线性 BGRA 缓冲
    size_t pitchRGBA, pitchBGRA;      // cudaMallocPitch 对齐的行间距
    cudaEvent_t releaseEvent;          // 同步事件
};
```

---

## 三、完整数据流（逐步）

### Step 1: DXGI 桌面采集
- `IDXGIOutputDuplication::AcquireNextFrame(10ms)` 获取桌面纹理
- `CopyResource` 拷贝到共享纹理（`D3D11_RESOURCE_MISC_SHARED`）
- `cudaGraphicsD3D11RegisterResource` 注册为 CUDA 资源（一次性）
- `cudaGraphicsMapResources` + `cudaGraphicsSubResourceGetMappedArray` → 得到 `cudaArray_t`（BGRA）

### Step 2: 鼠标光标渲染（CUDA Kernel）
- 通过 DXGI `GetFramePointerShape` 获取鼠标形状
- CUDA Kernel 直接在 cudaArray 上绘制光标（支持彩色/单色/Alpha蒙版）
- 处理显示器旋转的坐标变换

### Step 3: CUDA → OpenGL 纹理上传
- `cudaGraphicsGLRegisterImage(WriteDiscard)` 注册 GL 纹理为 CUDA 写入目标
- **BGRA 路径**：
  1. `cudaMemcpy2DFromArray` → 线性 bgraBuffer
  2. `bgraToRgbaKernel` → 线性 rgbaBuffer（通道交换）
  3. `cudaMemcpy2DToArray` → GL 纹理的 cudaArray
- **NV12 路径**：
1. `NV12ToRGBAKernel` → 线性 rgbaBuffer（YUV→RGB 色彩空间转换）
  2. `cudaMemcpy2DToArray` → GL 纹理的 cudaArray

### Step 4: OpenGL FBO 离屏合成
- 创建离屏 FBO + 纹理（与输出分辨率一致）
- 按 priority 排序遍历所有可见图层
- 每个图层绑定其 GL 纹理，用着色器绘制到 FBO
- 着色器支持旋转（通过 `rotateType` uniform 变换纹理坐标）
- FBO 着色器做 Y 轴翻转（适配编码器坐标系）

### Step 5: 编码器读取 FBO
- `cudaGraphicsGLRegisterImage(ReadOnly)` 注册 FBO 纹理为 CUDA 读取源
- `cudaGraphicsMapResources` + `cudaGraphicsSubResourceGetMappedArray` → 得到合成结果的 `cudaArray_t`
- `rgbaToNV12Kernel` 转换为 NV12 → 送入 NVENC 编码

### Step 6: 三路复用
- 预览：直接用 `QOpenGLWidget::paintGL()` 显示同一组 GL 纹理
- 录制/推流：共享同一个 FBO 输出

---

## 四、CUDA Kernel 实现细节

### NV12 → RGBA（BT.601）
```cpp
__global__ void NV12ToRGBAKernel(uint8_t* yPlane, uint8_t* uvPlane,
uint8_t* rgba, int width, int height,
      int yPitch, int uvPitch, int rgbaPitch) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    float Y  = yPlane[y * yPitch + x];
    float Cb = uvPlane[(y/2) * uvPitch + (x/2)*2] - 128.0f;
    float Cr = uvPlane[(y/2) * uvPitch + (x/2)*2 + 1] - 128.0f;

    // BT.601 转换矩阵
    float R = Y + 1.402f * Cr;
    float G = Y - 0.344136f * Cb - 0.714136f * Cr;
 float B = Y + 1.772f * Cb;

    int offset = y * rgbaPitch + x * 4;
    rgba[offset]   = clamp(R, 0, 255);
    rgba[offset+1] = clamp(G, 0, 255);
    rgba[offset+2] = clamp(B, 0, 255);
    rgba[offset+3] = 255;
}
// 线程块: dim3(16, 16), grid: (width/16, height/16)
```

### RGBA → NV12（BT.709 Full→Limited）
```cpp
// BT.709 矩阵 + Full Range → Limited Range 缩放
Y  = 0.2126*R + 0.7152*G + 0.0722*B;  → 映射到 [16, 235]
Cb = -0.1146*R - 0.3854*G + 0.5*B;    → 映射到 [16, 240]
Cr = 0.5*R - 0.4542*G - 0.0458*B;     → 映射到 [16, 240]
```

### BGRA → RGBA
```cpp
__global__ void bgraToRgbaKernel(uint8_t* bgra, uint8_t* rgba,
              int width, int height,
 int srcPitch, int dstPitch) {
    // 简单的通道交换: B↔R
    rgba[offset]   = bgra[offset+2];  // R ← B
    rgba[offset+1] = bgra[offset+1];  // G ← G
    rgba[offset+2] = bgra[offset];    // B ← R
    rgba[offset+3] = bgra[offset+3];  // A ← A
}
```

---

## 五、线程模型

| 线程 | 职责 | 关键操作 |
|------|------|----------|
| 采集线程 | DXGI 帧获取 + 鼠标渲染 | `AcquireNextFrame` / CUDA 鼠标 Kernel |
| 主线程(Qt) | OpenGL 渲染 + FBO 合成 | `makeCurrent` / `bindFbo` / `drawArrays` |
| 渲染定时器 | 控制合成帧率 | `RenderTimer` 发送自定义事件触发 `offscreenRender` |
| 编码线程 | CUDA 读 FBO + NV12 转换 + NVENC | `mapFboCudaArray` / `rgbaToNV12Kernel` |

---

## 六、同步机制

1. **cudaEvent**：`releaseEvent` 用于采集线程和渲染线程间的 GPU 操作同步
2. **原子变量**：`lastCompositePts`（atomic<int64_t>）保证 PTS 跨线程可见
3. **Qt 事件循环**：`RenderTimer` 通过 `QCoreApplication::postEvent` 驱动渲染
4. **OpenGL 上下文**：所有 GL 操作必须在主线程 `makeCurrent` 后执行

---

## 七、开发中遇到的问题与解决方案

### 问题 1：CUDA-OpenGL 互操作注册后纹理内容为空

**现象**：`cudaGraphicsGLRegisterImage` 成功，但 `cudaMemcpy2DToArray` 写入后 GL 纹理显示全黑。

**原因**：OpenGL 纹理创建时未指定 `GL_RGBA8` 内部格式，使用了默认的 `GL_RGBA`（驱动可能选择压缩格式），导致 CUDA 写入的数据布局与 GL 期望不匹配。

**解决**：创建纹理时显式指定 `glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr)`，确保 CUDA 和 OpenGL 对纹理内存布局的理解一致。

---

### 问题 2：多显示器旋转场景下采集画面错位

**现象**：竖屏显示器（90°旋转）采集后画面宽高互换，鼠标位置偏移。

**原因**：DXGI `GetDesc` 返回的是逻辑分辨率（旋转前），但实际纹理是物理分辨率（旋转后）。当显示器有 `DXGI_MODE_ROTATION_ROTATE90` 时，物理尺寸 = (logicalHeight, logicalWidth)。

**解决**：
```cpp
if (rotation == DXGI_MODE_ROTATION_ROTATE90 || rotation == DXGI_MODE_ROTATION_ROTATE270) {
physicalWidth = logicalHeight;
    physicalHeight = logicalWidth;
}
```
同时鼠标坐标也需要做相应的旋转变换。

---

### 问题 3：FBO 输出送编码器后画面上下颠倒

**现象**：预览正常，但录制/推流的视频画面垂直翻转。

**原因**：OpenGL 坐标系原点在左下角（Y 轴向上），而视频编码器期望的是左上角原点（Y 轴向下）。FBO 离屏渲染保留了 OpenGL 坐标系。

**解决**：为 FBO 着色器单独设计纹理坐标，在 vertex shader 中翻转 Y 轴：
```glsl
// FBO 专用 vertex shader
texCoord = vec2(aTexCoord.x, 1.0 - aTexCoord.y);  // Y轴翻转
```

---

### 问题 4：高帧率下 CUDA-GL 互操作偶发卡死

**现象**：60fps 持续运行一段时间后，`cudaGraphicsMapResources` 偶发阻塞数百毫秒。

**原因**：CUDA 和 OpenGL 共享资源时，如果 GL 正在使用该纹理（如正在绘制），CUDA map 操作会等待 GL 完成。在高帧率下，渲染和上传可能产生竞争。

**解决**：
1. 引入 `cudaEvent_t releaseEvent`，在 CUDA 写入完成后 record event
2. 渲染前 `cudaEventSynchronize` 确保 CUDA 写入完成
3. 使用 `cudaGraphicsRegisterFlagsWriteDiscard` 标志注册，允许 CUDA 覆盖写而不等待 GL 读取完成

---

### 问题 5：cudaMallocPitch 对齐导致颜色偏移

**现象**：BGRA→RGBA 转换后画面出现斜线条纹。

**原因**：`cudaMallocPitch` 返回的 pitch 可能大于 `width * 4`（为了对齐到 512 字节），但 Kernel 中使用 `width * 4` 作为行间距，导致每行读取偏移。

**解决**：Kernel 中统一使用 `cudaMallocPitch` 返回的 `pitchRGBA`/`pitchBGRA` 而非手动计算。

---

## 八、面试必须掌握的知识点

### 1. CUDA-OpenGL 互操作 API
- `cudaGraphicsGLRegisterImage`：注册 GL 纹理为 CUDA 资源
  - `WriteDiscard`：CUDA 写入，不保留原内容
  - `ReadOnly`：CUDA 只读
- `cudaGraphicsMapResources`：锁定资源供 CUDA 使用
- `cudaGraphicsSubResourceGetMappedArray`：获取 cudaArray 指针
- `cudaGraphicsUnmapResources`：释放锁定

### 2. CUDA-D3D11 互操作 API
- `cudaGraphicsD3D11RegisterResource`：注册 D3D11 纹理
- 与 GL 互操作的区别：D3D11 纹理本身是 2D Array（带 pitch），GL 纹理是线性的

### 3. OpenGL FBO 离屏渲染
- `glGenFramebuffers` / `glBindFramebuffer(GL_FRAMEBUFFER, fbo)`
- `glFramebufferTexture2D` 附加颜色纹理
- `glCheckFramebufferStatus` 验证完整性
- 离屏渲染不显示到屏幕，结果保留在纹理中

### 4. NV12 格式
- Y 平面：width × height，每像素 1 字节亮度
- UV 平面：width × height/2，每 2×2 像素共享一对 Cb/Cr
- 为什么用 NV12：硬件编码器（NVENC）原生支持，避免格式转换开销

### 5. 为什么不直接 D3D11 → NVENC？
- NVENC 需要 NV12 输入，DXGI 采集输出是 BGRA
- 需要多源合成（OBS 的场景概念），必须走合成管线
- OpenGL FBO 合成比 D3D11 Compute Shader 更适合 2D 图层混合（着色器更灵活）

### 6. 为什么选 OpenGL 而非 Vulkan？
- Qt 原生支持 OpenGL Widget，集成成本低
- 2D 合成场景 OpenGL 够用，Vulkan 的优势在 3D 和细粒度控制
- CUDA-OpenGL 互操作成熟稳定，文档和案例丰富

### 7. 零拷贝的含义
- "零拷贝"指 GPU 显存内的数据不经过 CPU 内存（无 Device→Host→Device 传输）
- 实际仍有 GPU 内部的 Array→Linear、Linear→Array 拷贝，但速度极快（GPU 带宽）
- 唯一可能的 CPU 参与：鼠标形状数据上传（很小，可忽略）

### 8. 性能关键指标
- 1080p 单帧合成：< 2ms（GPU 侧）
- 4 源场景 CPU 占用：~8%（对比纯 CPU 方案 35%）
- 内存占用：主要是 GPU 显存（每图层约 8MB RGBA 纹理）
