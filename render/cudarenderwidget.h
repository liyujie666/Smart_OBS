#ifndef CUDARENDERWIDGET_H
#define CUDARENDERWIDGET_H
#include "source/video/videosource.h"
#include "dxgi/dxgidesktopduplicator.h"
#include "infotools.h"
#include "framelayer.h"
#include "timer/rendertimer.h"
#include "sync/avsyncclock.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include "cudainterophelper.h"
#include <QMutex>
#include <QSurface>
#include <QWindow>
#include <QTimer>
#include <QOffscreenSurface>

class SceneManager;
class Scene;
class CudaRenderWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit CudaRenderWidget(QWidget *parent = nullptr);
    ~CudaRenderWidget();

    // void setInteropHelper(CudaInteropHelper* helper);
    int findLayerIndex(int sourceId) const;        // 根据源ID获取图层索引
    void removeLayerBySourceId(int sourceId);
    void sortLayersByPriority();                   // 根据优先级对图层排序
    void promoteLayerToTop(int index);


    void initFBO();
    void setRecording(bool record);
    void setVideoConfig(int width,int height,double fps);
    void setSyncClock(AVSyncClock* syncClock);
    void setDXGIDuplicator(DxgiDesktopDuplicator* dxgiDuplicator);
    void setSceneManager(SceneManager* manager) { sceneManager_ = manager; }
    void updateDisplayLogicalSize();

    GLuint fboTexture() const;
    CudaInteropHelper*  getEncoderCudaHelper() const;
    void releaseFBO();
    void offscreenRender();
    QOpenGLContext* getMainGLContext();

    // 离屏渲染
    void renderLayersToWindow();    // 向窗口绘制图层
    void renderBorders();           // 绘制边框（仅非录制状态）
    void renderSceneLayers(Scene* scene, const QSize& widgetSize);
    void renderSceneLayersOffscreen(Scene *scene, const QSize &fboSize);

    int64_t getLastRenderedFrameIndex() const;

    QOpenGLShaderProgram& getMainShader() { return shader_; }
    QOpenGLShaderProgram& getOffscreenShader() { return offscreenShader_; }
    void release();

public slots:
    // void onFrameReady(const CudaFrameInfo& info);
    void addOrUpdateLayer(const CudaFrameInfo& info);   // 添加或更新图层
    void setLayerVisible(int sourceId, bool visible);
    void setLayerLocked(int sourceId, bool locked);
    void setLayerSelected(int sourceId);

signals:
    void layerClicked(int sourceId);
    void frameRecorded(int64_t framePts);
    void recordingStopped();
    void initialized();
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 新增：鼠标事件处理
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:

    bool event(QEvent* e) override {
        if (e->type() == RenderTimer::RenderTriggerEvent) {
            offscreenRender(); // 主线程执行渲染
            return true;
        }

        if (!context() || !context()->isValid()) {
            return QWidget::event(e);   // 退化为普通 QWidget
        }
        return QOpenGLWidget::event(e); // 其他事件交给父类
    }

    DxgiDesktopDuplicator* dxgiDuplicator_ = nullptr;
    int displayWidth_ = 1920;  // 动态获取的显示器逻辑宽度
    int displayHeight_ = 1080; // 动态获取的显示器逻辑高度
    float displayRatio_ = 16.0f / 9.0f; // 动态计算的宽高比

    QOpenGLContext* mainGLContext_ = nullptr;
    SceneManager* sceneManager_ = nullptr;

    QOpenGLShaderProgram shader_;        // 主着色器
    QOpenGLShaderProgram borderShader_;  // 边框着色器
    QOpenGLShaderProgram fboShader_;     // fbo着色器
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint borderVao_ = 0;               // 边框VAO
    GLuint borderVbo_ = 0;               // 边框VBO
    // QVector<FrameLayer*> layers_;        // 图层列表
    // int selectedLayerIndex_ = -1;        // 当前选中的图层索引
    // QMap<int, FrameLayer*> sourceIdToLayerMap;
    QPointF lastMousePos;                // 上次鼠标位置



    GLuint fbo_ = 0;
    GLuint fboTexture_ = 0;
    bool isRecording_ = false;
    int fboWidth_ = 0;
    int fboHeight_ = 0;
    double fboFPS_ = 30.0;
    int dragIntervalMs_ = 16;
    CudaInteropHelper* encoderCudaHelper_;
    mutable std::mutex cudaHelperMutex_;
    QOffscreenSurface* offscreenSurface_ = nullptr;
    QOpenGLContext* glContext_ = nullptr;
    RenderTimer* m_renderTimer = nullptr;
    QTimer* dragRenderTimer_ = nullptr;

    // 离屏渲染专用资源（与窗口渲染完全隔离）
    QOpenGLShaderProgram offscreenShader_;  // 离屏渲染着色器
    GLuint offscreenVAO_ = 0;
    GLuint offscreenVBO_ = 0;
    GLuint offscreenBorderVAO_ = 0;
    GLuint offscreenBorderVBO_ = 0;
    QOpenGLShaderProgram offscreenBorderShader_;  // 离屏边框着色器


    int64_t frameIntervalUs = 0;    // 固定帧间隔（微秒）
    int64_t basePts = 0;            // 基准PTS（由首个有效图层时间戳确定）
    int64_t frameCount = 0;             // 累计帧数

    AVSyncClock* syncClock_ = nullptr;
    std::atomic<int64_t> lastCompositePts{0}; // 原子变量，确保线程间可见性
    std::atomic<bool> isFirstFrame{true};
};


#endif // CUDARENDERWIDGET_H
