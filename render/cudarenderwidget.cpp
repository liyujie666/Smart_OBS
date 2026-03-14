// cudarenderwidget.cpp
#include "cudarenderwidget.h"
#include "sync/offsetmanager.h"
#include "scene/scenemanager.h"
#include "scene/scene.h"
#include "sync/globalclock.h"
#include "transition/transitionmanager.h"
#include <QOpenGLContext>
#include <QDebug>

// 边框顶点着色器
static const char* borderVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 pos;
uniform mat4 transform;
void main() {
    gl_Position = transform * vec4(pos, 0.0, 1.0);
}
)";
// 边框片段着色器
static const char* borderFragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 color;
void main() {
    FragColor = color;
}
)";
static const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 texCoord;
out vec2 v_texCoord;

uniform int rotateType;
uniform float alpha;
uniform vec2 offset;

void main() {
    if (rotateType == 1) {
        // 90度旋转：交换XY并翻转新Y轴（原X轴），解决上下颠倒
        v_texCoord = vec2(texCoord.y, 1.0 - texCoord.x); // 核心修正：添加1.0 -
    } else if (rotateType == 2) {
        // 270度旋转：交换XY并翻转新X轴（原Y轴）
        v_texCoord = vec2(texCoord.y, texCoord.x); // 保持不变（已适配）
    } else {
        // 正常显示：翻转Y轴
        v_texCoord = vec2(texCoord.x, 1.0 - texCoord.y);
    }
    gl_Position = vec4(pos + offset, 0.0, 1.0);
}
)";
static const char* fragmentShaderSrc = R"(
#version 330 core
in vec2 v_texCoord;
out vec4 FragColor;
uniform sampler2D tex;
uniform float alpha;
void main() {
    FragColor = texture(tex, v_texCoord);
    FragColor.a *= alpha;
}
)";
static const char* vertexFboShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_texCoord;
out vec2 v_texCoord;
uniform int rotateType;
uniform vec2 offset;

void main() {
    gl_Position = vec4(a_pos + offset, 0.0, 1.0);
    if (rotateType == 1) {
        // 90度旋转：保持现有逻辑
        v_texCoord = vec2(a_texCoord.y, 1.0 - a_texCoord.x);
    } else if (rotateType == 2) {
        // 270度旋转：通过翻转X和Y轴抵消180度旋转
        v_texCoord = vec2(1.0 - a_texCoord.y, a_texCoord.x); // 核心修正
    } else {
        v_texCoord = vec2(a_texCoord.x,a_texCoord.y);
    }
}
)";
static const char* fragmentFboShaderSrc = R"(
#version 330 core
in vec2 v_texCoord;
uniform sampler2D tex;
out vec4 fragColor;
uniform float alpha;
void main() {
    fragColor = texture(tex, v_texCoord); // 采样翻转后的坐标
    fragColor.a *= alpha;
}
)";

CudaRenderWidget::CudaRenderWidget(QWidget *parent)
    : QOpenGLWidget(parent),encoderCudaHelper_(nullptr)
{

    setMouseTracking(true);
    dragRenderTimer_ = new QTimer(this);
    dragRenderTimer_->setTimerType(Qt::CoarseTimer);
    connect(dragRenderTimer_,&QTimer::timeout,this,[this]{
        this->update();
    });


}

CudaRenderWidget::~CudaRenderWidget()
{
    release();
}
void CudaRenderWidget::addOrUpdateLayer(const CudaFrameInfo& info)
{
    if (!sceneManager_) {
        qWarning() << "CudaRenderWidget: 未设置SceneManager，无法添加图层";
        return;
    }

    TransitionManager* tm = TransitionManager::getInstance();
    std::shared_ptr<Scene> oldScene = tm->isTransitioning() ? tm->oldScene() : nullptr;
    std::shared_ptr<Scene> newScene = tm->isTransitioning() ? tm->newScene() : nullptr;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (info.sceneId != currentScene->id() && info.sceneId != (oldScene ? oldScene->id() : -1) && info.sceneId != (newScene ? newScene->id() : -1))
        return;
    if (!currentScene && !oldScene && !newScene) return;

    // 找到帧对应的场景（转场期优先用旧/新场景，否则用 currentScene）
    std::shared_ptr<Scene> targetScene = nullptr;
    if (currentScene && info.sceneId == currentScene->id())
        targetScene = currentScene;
    else if (oldScene && info.sceneId == oldScene->id())
        targetScene = oldScene;
    else if (newScene && info.sceneId == newScene->id())
        targetScene = newScene;
    else
        return;

    makeCurrent();

    FrameLayer* layer = targetScene->findLayerById(info.sourceId);
    if (!layer) {
        layer = new FrameLayer();
        layer->frameInfo = info;
        layer->interopHelper = new CudaInteropHelper();

        if (info.type == VideoSourceType::Desktop) {
            if (dxgiDuplicator_) {
                layer->selfAspectRatio = (float)dxgiDuplicator_->logicalWidth() / dxgiDuplicator_->logicalHeight();
                if (dxgiDuplicator_->rotation() == DXGI_MODE_ROTATION_ROTATE90)
                    layer->selfRotateType = 1;
                else if (dxgiDuplicator_->rotation() == DXGI_MODE_ROTATION_ROTATE270)
                    layer->selfRotateType = 2;
            }
        } else {
            layer->selfAspectRatio = (float)info.width / info.height;
            layer->selfRotateType = 0;
        }

        auto sceneLayers = targetScene->sceneLayers();
        float widthRatio = 0.5f;
        float heightRatio = widthRatio / layer->selfAspectRatio;
        if (heightRatio > 0.9f) heightRatio = 0.9f;

        layer->rectRatio = QRectF(0.05f + sceneLayers.size() * 0.05f,
                                  0.05f + sceneLayers.size() * 0.05f,
                                  widthRatio, heightRatio);
        layer->updateAbsoluteRect(width(), height());
        layer->updateRatioFromAbsolute(width(), height());
        targetScene->addLayer(layer);
    } else {
        if (!layer->isActive) { doneCurrent(); return; }
        layer->frameInfo = info;

        if (info.type == VideoSourceType::Text &&
            (info.width != layer->interopHelper->width() ||
             info.height != layer->interopHelper->height())) {
            layer->interopHelper->initInterop(info.width, info.height);
            layer->selfAspectRatio = (float)info.width / info.height;
            QRectF oldRect = layer->rect;
            float newHeight = oldRect.width() / layer->selfAspectRatio;
            layer->rect = QRectF(oldRect.x(), oldRect.y(), oldRect.width(), newHeight);
            layer->updateRatioFromAbsolute(width(), height());
        }
    }

    if (!layer->interopHelper->isInitialized())
        layer->interopHelper->initInterop(info.width, info.height);

    if (info.format == FrameFormat::NV12){
        layer->interopHelper->uploadNV12ToGL(info.yPlane, info.uvPlane,info.pitchY, info.pitchUV);
    }
    else if (info.format == FrameFormat::BGRA){
        layer->interopHelper->uploadBGRAToGL(info.bgraArray);
    }

    doneCurrent();
    targetScene->sortLayersByPriority();
    update();
}

void CudaRenderWidget::setLayerVisible(int sourceId, bool visible) {
    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 从当前场景查找图层并设置可见性
    FrameLayer* layer = currentScene->findLayerById(sourceId);
    if (layer) {
        layer->setVisible(visible);
        update();
    }
}

void CudaRenderWidget::setLayerLocked(int sourceId, bool locked) {
    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 从当前场景查找图层并设置可见性
    FrameLayer* layer = currentScene->findLayerById(sourceId);
    if (layer) {
        layer->setLocked(locked);
        update();
    }
}

void CudaRenderWidget::setLayerSelected(int sourceId)
{
    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    auto sceneLayers = currentScene->sceneLayers();
    for (auto& layer : sceneLayers) {
        if(layer->frameInfo.sourceId == sourceId)
            layer->isSelected = true;
        else
            layer->isSelected = false;
    }

    update();
}


// FrameLayer* CudaRenderWidget::findLayerBySourceId(int sourceId) {
//     auto it = sourceIdToLayerMap.find(sourceId);
//     if (it != sourceIdToLayerMap.end())
//         return it.value();
//     return nullptr;
// }

// int CudaRenderWidget::findLayerIndex(int sourceId) const
// {
//     for (int i = 0; i < layers_.size(); i++) {
//         if (layers_[i]->frameInfo.sourceId == sourceId)
//             return i;
//     }
//     return -1;
// }

void CudaRenderWidget::removeLayerBySourceId(int sourceId)
{

    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 从当前场景删除图层（场景内部会释放资源）

    makeCurrent(); // 激活 OpenGL 上下文

    currentScene->removeLayer(sourceId);

    // 3. 清空当前帧缓冲，避免残留
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    doneCurrent();
    update(); // 触发重绘，确认无残留

}

// void CudaRenderWidget::sortLayersByPriority()
// {
//     std::sort(layers_.begin(), layers_.end(),
//               [](const FrameLayer* a, const FrameLayer* b) {
//                   return a->frameInfo.priority > b->frameInfo.priority;
//               });
// }

QOpenGLContext* CudaRenderWidget::getMainGLContext() {
    return mainGLContext_;
}


void CudaRenderWidget::offscreenRender()
{
    // 仅在录屏时执行
    if (!isRecording_ || !syncClock_->isValid() || !sceneManager_) return;

    TransitionManager* tm = TransitionManager::getInstance();

    makeCurrent();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, fboWidth_, fboHeight_);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);


    if (tm->isTransitioning() && tm->getCurrentTransition()) {

        auto fixRectForFbo = [&](Scene* s) {
            if (!s) return;
            for (auto l : s->sceneLayers()) {
                const QRectF& r = l->rect;                    // 窗口像素
                float x = r.x() * fboWidth_  / width();
                float y = r.y() * fboHeight_ / height();
                float w = r.width()  * fboWidth_  / width();
                float h = w / l->selfAspectRatio;             // 保持自身宽高比
                l->rect = QRectF(x, y, w, h);
            }
        };
        fixRectForFbo(tm->oldScene().get());
        fixRectForFbo(tm->newScene().get());

        tm->getCurrentTransition()->render(this, QSize(fboWidth_, fboHeight_),RenderTarget::FBO);

        // 恢复窗口
        auto restoreRect = [&](Scene* s) {
            if (!s) return;
            for (auto l : s->sceneLayers())
                l->updateAbsoluteRect(width(), height());
        };
        restoreRect(tm->oldScene().get());
        restoreRect(tm->newScene().get());

        std::vector<int64_t> visibleLayerTs;
        auto collect = [&](Scene* s) {
            if (!s) return;
            for (auto l : s->sceneLayers())
                if (l->isVisible && l->isActive && l->interopHelper && l->interopHelper->isInitialized())
                    visibleLayerTs.push_back(l->frameInfo.timestamp);
        };
        collect(tm->oldScene().get());
        collect(tm->newScene().get());

        if (!visibleLayerTs.empty()) {
            int64_t maxLayerTs = *std::max_element(visibleLayerTs.begin(), visibleLayerTs.end());
            syncClock_->calibrateVideo(maxLayerTs);
        }
        int64_t compositePts = syncClock_->getVPts();
        if (compositePts <= lastCompositePts) compositePts = lastCompositePts + 1;
        lastCompositePts = compositePts;
        emit frameRecorded(compositePts);

        glFlush();
        GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        if (fence) {
            const GLuint64 timeout = 100000000; // 100 ms
            GLenum waitRes = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, timeout);
            if (waitRes == GL_TIMEOUT_EXPIRED || waitRes == GL_WAIT_FAILED) {
                qWarning() << "glClientWaitSync timed out or failed, falling back to glFinish()";
                glFinish();
            }
            glDeleteSync(fence);
        } else {
            glFinish();
        }
        return;
    }


    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    if (isFirstFrame) {
        isFirstFrame = false;
        frameCount = 1;
        lastCompositePts = 0;
        qDebug() << "[首次渲染] 强制重置lastCompositePts=0";
    }
    std::vector<int64_t> visibleLayerTs;
    auto sceneLayers = currentScene->sceneLayers();
    for (auto layer : sceneLayers) {
        if (!layer->isVisible || !layer->isActive || !layer->interopHelper || !layer->interopHelper->isInitialized())
            continue;
        OffsetManager::getInstance().processVideoOffset(layer->frameInfo, OffsetManager::getInstance().getConfig());
        visibleLayerTs.push_back(layer->frameInfo.timestamp);
        layer->interopHelper->waitForOperationsToComplete();

        GLuint tex = layer->interopHelper->texture();
        if (tex == 0) continue;

        QRectF wr = layer->rect; // 窗口中的像素坐标（左上原点）
        QRectF fbor(
            wr.x() * fboWidth_ / width(),
            wr.y() * fboHeight_ / height(),
            wr.width() * fboWidth_ / width(),
            wr.height() * fboHeight_ / height()
            );
        fbor.setHeight(fbor.width() / layer->selfAspectRatio);
        glViewport(int(fbor.x()), int(fbor.y()), int(fbor.width()), int(fbor.height()));
        offscreenShader_.bind();
        offscreenShader_.setUniformValue("rotateType", layer->selfRotateType);
        offscreenShader_.setUniformValue("alpha", 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        offscreenShader_.setUniformValue("tex", 0);
        glBindVertexArray(offscreenVAO_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        offscreenShader_.release();
    }
    glLineWidth(1.0f);
    glFlush();
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (fence) {
        const GLuint64 timeout = 100000000;
        GLenum waitRes = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, timeout);
        if (waitRes == GL_TIMEOUT_EXPIRED || waitRes == GL_WAIT_FAILED) {
            qWarning() << "glClientWaitSync timed out or failed, falling back to glFinish()";
            glFinish();
        }
        glDeleteSync(fence);
    } else {
        glFinish();
    }
    if (!visibleLayerTs.empty()) {
        int64_t maxLayerTs = *std::max_element(visibleLayerTs.begin(), visibleLayerTs.end());
        syncClock_->calibrateVideo(maxLayerTs);
    }
    int64_t compositePts = syncClock_->getVPts();

    if (compositePts <= lastCompositePts) compositePts = lastCompositePts + 1;
    lastCompositePts = compositePts;
    emit frameRecorded(compositePts);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    doneCurrent();
}
void CudaRenderWidget::initializeGL()
{
    initializeOpenGLFunctions();
    mainGLContext_ = context();

    updateDisplayLogicalSize();
    // 编译和链接主着色器
    shader_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc);
    shader_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSrc);
    shader_.link();

    // 编译和链接边框着色器
    borderShader_.addShaderFromSourceCode(QOpenGLShader::Vertex, borderVertexShaderSrc);
    borderShader_.addShaderFromSourceCode(QOpenGLShader::Fragment, borderFragmentShaderSrc);
    borderShader_.link();

    // 编译和链接FBO离屏渲染着色器
    offscreenShader_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexFboShaderSrc);
    offscreenShader_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentFboShaderSrc);
    offscreenShader_.link();

    // 窗口渲染
    // 设置全屏四边形
    float vertices[] = {
        // 位置      // 纹理坐标 (翻转 Y)
        -1.0f, -1.0f, 0.0f, 0.0f,  // 左下：纹理Y=0.0（原始）
        1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,  // 左上：纹理Y=1.0（原始）
        1.0f,  1.0f, 1.0f, 1.0f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // 设置边框顶点
    float borderVertices[] = {
        -1.0f,  1.0f,  // 左上
        1.0f,  1.0f,  // 右上
        1.0f, -1.0f,  // 右下
        -1.0f, -1.0f   // 左下
    };

    glGenVertexArrays(1, &borderVao_);
    glGenBuffers(1, &borderVbo_);
    glBindVertexArray(borderVao_);
    glBindBuffer(GL_ARRAY_BUFFER, borderVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(borderVertices), borderVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    // 离屏渲染资源（录屏用）
    // 离屏渲染用VAO/VBO（独立顶点数据，避免共享）
    float offscreenVertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,  // 左下：纹理Y=0.0（原始）
        1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,  // 左上：纹理Y=1.0（原始）
        1.0f,  1.0f, 1.0f, 1.0f,
    };
    glGenVertexArrays(1, &offscreenVAO_);
    glGenBuffers(1, &offscreenVBO_);
    glBindVertexArray(offscreenVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, offscreenVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(offscreenVertices), offscreenVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    glEnable(GL_BLEND);          // 开启混合
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 初始化FBO
    initFBO();

    connect(TransitionManager::getInstance(), &TransitionManager::transitionProgressUpdated, this, [this]() {
        if (isVisible()) this->update();  // 窗口可见时才刷新
    });

    emit initialized();
}



void CudaRenderWidget::resizeGL(int w, int h)
{
    // 窗口最小化时（w或h为0），不更新FBO尺寸
    if (w == 0 || h == 0) return;

    if (dxgiDuplicator_) {
        updateDisplayLogicalSize();
    }

    glViewport(0, 0, w, h);
    if (!sceneManager_) return;
    auto sceneLayers = sceneManager_->currentScene()->sceneLayers();
    for (auto& layer : sceneLayers) {
        layer->updateAbsoluteRect(w, h);
    }

    // 仅在非录制状态下更新FBO尺寸（录制时保持固定尺寸）
    if (!isRecording_) {
        initFBO();
    }
}

void CudaRenderWidget::paintGL()
{
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, width(), height());
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    TransitionManager* tm = TransitionManager::getInstance();
    // 转场中：渲染转场帧；否则：正常渲染
    if (tm->isTransitioning() && tm->getCurrentTransition()) {
        tm->getCurrentTransition()->render(this, size(),RenderTarget::Window);
    } else {
        renderLayersToWindow();  // 正常渲染
    }
    renderBorders();         // 窗口专用边框渲染
}

void CudaRenderWidget::renderLayersToWindow()
{
    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 获取当前场景的所有图层
    auto sceneLayers = currentScene->sceneLayers();

    for (auto layer : sceneLayers) {
        if (!layer->isVisible || !layer->isActive || !layer->interopHelper || !layer->interopHelper->isInitialized())
            continue;

        layer->interopHelper->waitForOperationsToComplete();
        GLuint tex = layer->interopHelper->texture();
        if (tex == 0) continue;

        QRectF r = layer->rect;
        glViewport(int(r.x()), int(height() - r.y() - r.height()), int(r.width()), int(r.height()));
        shader_.bind();

        // 判断显示器旋转
        shader_.setUniformValue("rotateType", layer->selfRotateType);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        shader_.setUniformValue("tex", 0);
        shader_.setUniformValue("alpha",1.0f);
        shader_.setUniformValue("offset", QVector2D(0.0f, 0.0f));
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        shader_.release();
    }
}

// 新增：仅在非录制状态绘制边框
void CudaRenderWidget::renderBorders()
{

    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 获取当前场景的所有图层
    auto sceneLayers = currentScene->sceneLayers();

    glViewport(0, 0, width(), height());
    glLineWidth(3.0f);

    for (auto layer : sceneLayers) {
        if (!layer->isSelected) continue;

        QRectF r = layer->rect;
        glViewport(int(r.x()), int(height() - r.y() - r.height()), int(r.width()), int(r.height()));
        borderShader_.bind();
        borderShader_.setUniformValue("transform", QMatrix4x4());
        borderShader_.setUniformValue("color", QVector4D(0, 1, 0, 1));
        glBindVertexArray(borderVao_);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        glBindVertexArray(0);
        borderShader_.release();
    }
    glLineWidth(1.0f);
}

void CudaRenderWidget::renderSceneLayers(Scene *scene, const QSize &widgetSize)
{
    if (!scene) return;

    QOpenGLShaderProgram& mainShader = getMainShader();

    // 保存原始视口
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    auto sceneLayers = scene->sceneLayers();
    for (auto layer : sceneLayers) {
        if (!layer->isVisible || !layer->isActive || !layer->interopHelper || !layer->interopHelper->isInitialized())
            continue;

        layer->interopHelper->waitForOperationsToComplete();
        GLuint tex = layer->interopHelper->texture();
        if (tex == 0) continue;

        // 设置当前图层的视口
        QRectF r = layer->rect;
        glViewport(
            int(r.x()),
            int(widgetSize.height() - r.y() - r.height()),
            int(r.width()),
            int(r.height())
            );

        // 渲染图层
        mainShader.bind();
        mainShader.setUniformValue("rotateType", layer->selfRotateType);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        mainShader.setUniformValue("tex", 0);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        // 解绑资源（避免残留状态）
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        mainShader.release();
    }

    //  恢复原始视口
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

void CudaRenderWidget::renderSceneLayersOffscreen(Scene *scene, const QSize &fboSize)
{
    if (!scene) return;

    // 离屏模式：强制使用offscreenShader_和offscreenVAO_
    QOpenGLShaderProgram& offscreenShader = getOffscreenShader(); // 新增获取离屏着色器的接口

    // 保存原始视口（FBO视口是0,0,fboWidth_,fboHeight_，无需额外翻转）
    GLint oldViewport[4];
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    auto sceneLayers = scene->sceneLayers();
    for (auto layer : sceneLayers) {
        if (!layer->isVisible || !layer->isActive || !layer->interopHelper || !layer->interopHelper->isInitialized())
            continue;

        layer->interopHelper->waitForOperationsToComplete();
        GLuint tex = layer->interopHelper->texture();
        if (tex == 0) continue;

        // 离屏模式：视口直接用FBO坐标系（左下角原点，无需Y轴翻转）
        QRectF r = layer->rect;
        glViewport(
            int(r.x()),    // X：直接用FBO中的X（无需调整）
            int(r.y()),    // Y：直接用FBO中的Y（左下角原点，不翻转）
            int(r.width()),
            int(r.height())
            );

        // 离屏模式：绑定offscreenShader和offscreenVAO
        offscreenShader.bind();
        offscreenShader.setUniformValue("rotateType", layer->selfRotateType); // 离屏着色器已适配旋转
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        offscreenShader.setUniformValue("tex", 0);
        glBindVertexArray(offscreenVAO_); // 强制用离屏VAO
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // 解绑资源，避免污染
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        offscreenShader.release();
    }

    // 恢复原始视口
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
}

void CudaRenderWidget::release()
{

    if (m_renderTimer) {
        m_renderTimer->stopTimer();  // 停止定时器
        delete m_renderTimer;        // 释放定时器对象
        m_renderTimer = nullptr;
    }

    if(dragRenderTimer_)
    {
        dragRenderTimer_->stop();
        delete dragRenderTimer_;
        dragRenderTimer_ = nullptr;
    }

    makeCurrent();

    if (encoderCudaHelper_) {
        encoderCudaHelper_->release();  // 释放FBO纹理的CUDA注册资源、CUDA内存等
        delete encoderCudaHelper_;      // 释放helper对象
        encoderCudaHelper_ = nullptr;
    }
    releaseFBO();

    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (borderVao_) {
        glDeleteVertexArrays(1, &borderVao_);
        borderVao_ = 0;
    }
    if (borderVbo_) {
        glDeleteBuffers(1, &borderVbo_);
        borderVbo_ = 0;
    }
    if (offscreenVAO_) {
        glDeleteVertexArrays(1, &offscreenVAO_);
        offscreenVAO_ = 0;
    }
    if (offscreenVBO_) {
        glDeleteBuffers(1, &offscreenVBO_);
        offscreenVBO_ = 0;
    }

    shader_.release();
    borderShader_.release();
    offscreenShader_.release();


    dxgiDuplicator_ = nullptr;  // dxgiDuplicator_由外部创建，不负责销毁
    syncClock_ = nullptr;       // syncClock_由外部创建，不负责销毁

    isRecording_ = false;
    isFirstFrame = true;
    lastCompositePts = 0;
    frameCount = 0;
    fboWidth_ = 0;
    fboHeight_ = 0;
    fboFPS_ = 0.0;
    dragIntervalMs_ = 0;

    doneCurrent();
}

void CudaRenderWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    QPointF pos = event->position();
    lastMousePos = pos;

    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    // 重置状态，但不要清掉已经在拖拽/缩放的状态（刚按下前通常都为 false）
    auto sceneLayers = currentScene->sceneLayers();
    for (auto layer : sceneLayers) {
        layer->isSelected = false;
        layer->isDragging = false;
        layer->isResizing = false;
    }

    // 从上到下命中手柄或图层
    for (auto it = sceneLayers.rbegin(); it != sceneLayers.rend(); ++it) {
        FrameLayer* layer = *it;
        if (layer->isLocked) continue;

        int resizeHandle = layer->hitTestResizeHandle(pos);
        if (resizeHandle > 0) {
            layer->isSelected = true;
            layer->isResizing = true;
            layer->resizeHandle = resizeHandle;
            layer->hasSavedRect = false;
            grabMouse();
            if(dragRenderTimer_) dragRenderTimer_->start(dragIntervalMs_);
            update();
            return;
        }

        if (layer->contains(pos)) {
            layer->isSelected = true;
            layer->isDragging = true;
            layer->dragOffset = pos - QPointF(layer->rect.x(), layer->rect.y());
            emit layerClicked(layer->frameInfo.sourceId);
            grabMouse();
            if(dragRenderTimer_) dragRenderTimer_->start(dragIntervalMs_);
            update();
            return;
        }
    }

    update();
}

void CudaRenderWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPointF pos = event->position();
    QPointF delta = pos - lastMousePos;
    lastMousePos = pos;

    if (!sceneManager_) {
        setCursor(Qt::ArrowCursor);
        return;
    }
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) {
        setCursor(Qt::ArrowCursor);
        return;
    }

    auto sceneLayers = currentScene->sceneLayers();

    // 1) 优先找到当前处于 active 状态的图层（isDragging 或 isResizing）
    FrameLayer* activeLayer = nullptr;
    for (auto it = sceneLayers.rbegin(); it != sceneLayers.rend(); ++it) {
        FrameLayer* layer = *it;
        if (layer->isDragging || layer->isResizing) {
            activeLayer = layer;
            break;
        }
    }

    if (activeLayer) {
        // 如果锁定则直接忽略并解绑抓取（保护性措施）
        if (activeLayer->isLocked) {
            setCursor(Qt::ForbiddenCursor);
            return;
        }

        // 实际拖动/缩放逻辑，不再依赖当前位置的 hitTest
        if (activeLayer->isDragging) {
            activeLayer->rect.translate(delta);
            // activeLayer->updateRatioFromAbsolute(width(), height());
            setCursor(Qt::SizeAllCursor);
        } else if (activeLayer->isResizing) {
            activeLayer->resizeByHandle(activeLayer->resizeHandle, delta);
            // activeLayer->updateRatioFromAbsolute(width(), height());
            // 保持合适的鼠标形状（尽量与手柄方向匹配）
            int handle = activeLayer->resizeHandle;
            switch (handle) {
            case 1: case 5: setCursor(Qt::SizeFDiagCursor); break;
            case 3: case 7: setCursor(Qt::SizeBDiagCursor); break;
            case 2: case 6: setCursor(Qt::SizeVerCursor);   break;
            case 4: case 8: setCursor(Qt::SizeHorCursor);   break;
            default: setCursor(Qt::ArrowCursor); break;
            }
        }
        // update();
        return;
    }

    // 2) 如果没有 active layer，再做常规的 hover hit 测试来设置光标（不改变任何 isDragging/isResizing）
    bool found = false;
    for (auto it = sceneLayers.rbegin(); it != sceneLayers.rend(); ++it) {
        FrameLayer* layer = *it;
        if (!layer->contains(pos) && layer->hitTestResizeHandle(pos) <= 0)
            continue;

        if (layer->isLocked) {
            setCursor(Qt::ForbiddenCursor);
            found = true;
            break;
        }

        int handle = layer->hitTestResizeHandle(pos);
        if (handle > 0) {
            switch (handle) {
            case 1: case 5: setCursor(Qt::SizeFDiagCursor); break;
            case 3: case 7: setCursor(Qt::SizeBDiagCursor); break;
            case 2: case 6: setCursor(Qt::SizeVerCursor);   break;
            case 4: case 8: setCursor(Qt::SizeHorCursor);   break;
            }
            found = true;
            break;
        } else {
            setCursor(layer->isSelected ? Qt::SizeAllCursor : Qt::ArrowCursor);
            found = true;
            break;
        }
    }

    if (!found)
        setCursor(Qt::ArrowCursor);
}

void CudaRenderWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (!event || event->button() != Qt::LeftButton)
        return;

    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    auto sceneLayers = currentScene->sceneLayers();
    for (FrameLayer* layer : sceneLayers) {
        if (layer && (layer->isDragging || layer->isResizing)) {
            layer->isDragging = false;
            layer->isResizing = false;
            layer->updateRatioFromAbsolute(width(), height());
            break; // 只处理第一个处于操作状态的图层
        }
    }
    if (dragRenderTimer_ && dragRenderTimer_->isActive()) {
        dragRenderTimer_->stop();
    }
    // 释放鼠标抓取
    releaseMouse();
    update();
}



void CudaRenderWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!event || event->button() != Qt::LeftButton) return;
    if (!sceneManager_) return;
    std::shared_ptr<Scene> currentScene = sceneManager_->currentScene();
    if (!currentScene) return;

    FrameLayer* selectedLayer = nullptr;
    auto sceneLayers = currentScene->sceneLayers();
    for (FrameLayer* layer : sceneLayers) {
        if (layer && layer->isSelected) { selectedLayer = layer; break; }
    }
    if (!selectedLayer || selectedLayer->isLocked) return;

    const float aspect = selectedLayer->selfAspectRatio;
    if (qFuzzyIsNull(aspect)) return;

    const float widgetW = static_cast<float>(width());
    const float widgetH = static_cast<float>(height());
    QRectF newRect;

    if (!selectedLayer->hasSavedRect) {
        // 保存当前 rect 以便还原
        selectedLayer->savedRect = selectedLayer->rect;
        selectedLayer->hasSavedRect = true;

        // Fit 到 widget（不留边距），保持宽高比
        float widgetAspect = widgetW / widgetH;
        if (widgetAspect > aspect) {
            float h = widgetH;
            float w = h * aspect;
            newRect = QRectF((widgetW - w)/2.0f, 0.0f, w, h);
        } else {
            float w = widgetW;
            float h = w / aspect;
            newRect = QRectF(0.0f, (widgetH - h)/2.0f, w, h);
        }
    } else {
        // 恢复之前保存的 rect（还原）
        newRect = selectedLayer->savedRect;
        selectedLayer->hasSavedRect = false;
    }

    if (newRect.width() > 0 && newRect.height() > 0) {
        selectedLayer->rect = newRect;
        selectedLayer->updateRatioFromAbsolute(static_cast<int>(widgetW), static_cast<int>(widgetH));
        update();
    }
}


void CudaRenderWidget::initFBO()
{
    releaseFBO(); // 先释放旧资源
    std::lock_guard<std::mutex> lock(cudaHelperMutex_);
    qDebug() << "fboWidth_" << fboWidth_ << "fboHeight_" << fboHeight_;
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &fboTexture_);
    glBindTexture(GL_TEXTURE_2D, fboTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboWidth_, fboHeight_, 0,GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboWidth_, fboHeight_, 0,GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, fboTexture_, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qDebug() << "FBO 创建失败";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    encoderCudaHelper_ = new CudaInteropHelper;
    encoderCudaHelper_->registerFboTexture(fboTexture_,fboWidth_,fboHeight_);

}

void CudaRenderWidget::setRecording(bool record)
{
    if (isRecording_ == record)
        return;

    isRecording_ = record;

    // 初始化定时器（保持原有逻辑）
    if (!m_renderTimer) {
        double intervalUs = 1000000.0 / fboFPS_;
        m_renderTimer = new RenderTimer(this, intervalUs, this);
    }

    if (record) {
        // 启动定时器开始录制
        m_renderTimer->startTimer();
        if(fbo_ == 0) initFBO();

    } else {
        // 停止录制时清理（保持原有逻辑）
        if (m_renderTimer) {
            m_renderTimer->stopTimer();
            delete m_renderTimer;
            m_renderTimer = nullptr;
        }

        // 重置帧状态
        isFirstFrame.store(true, std::memory_order_seq_cst);
        lastCompositePts.store(0, std::memory_order_seq_cst);
        frameCount = 0;
        emit recordingStopped();
    }
}



void CudaRenderWidget::setVideoConfig(int width, int height, double fps)
{
    if (fboWidth_ == width && fboHeight_ == height && fboFPS_ == fps) {
        return;
    }

    fboWidth_ = width;
    fboHeight_ = height;
    fboFPS_ = fps;

    qDebug() << "更新FBO尺寸为：" << fboWidth_ << "x" << fboHeight_;

    if (isInitialized()) {
        makeCurrent();
        initFBO();
        doneCurrent();
    }
}
void CudaRenderWidget::setSyncClock(AVSyncClock *syncClock)
{
    syncClock_ = syncClock;
}

void CudaRenderWidget::setDXGIDuplicator(DxgiDesktopDuplicator *dxgiDuplicator)
{
    dxgiDuplicator_ = dxgiDuplicator;

    updateDisplayLogicalSize();
    // 尺寸变化后，重新初始化FBO和图层
    if (isInitialized() && !isRecording_) {
        initFBO();
        if (sceneManager_)
        {
            auto sceneLayers =sceneManager_->currentScene()->sceneLayers();
            for (auto& layer : sceneLayers) {
                layer->logicalAspectRatio = displayRatio_;
                layer->updateAbsoluteRect(width(), height());
            }
        }
        update();
    }
}

void CudaRenderWidget::updateDisplayLogicalSize()
{
    if(!dxgiDuplicator_) return;

    displayWidth_ = dxgiDuplicator_->logicalWidth();
    displayHeight_ = dxgiDuplicator_->logicalHeight();

    if (displayHeight_ > 0) {
        displayRatio_ = static_cast<float>(displayWidth_) / displayHeight_;
    }

    // qDebug() << "动态获取显示器逻辑尺寸：" << displayWidth_ << "x" << displayHeight_
    //          << "，宽高比：" << displayRatio_;
}


GLuint CudaRenderWidget::fboTexture() const
{
    return fboTexture_;
}

CudaInteropHelper* CudaRenderWidget::getEncoderCudaHelper() const
{
    std::lock_guard<std::mutex> lock(cudaHelperMutex_);
    return encoderCudaHelper_;
}

void CudaRenderWidget::releaseFBO()
{
    std::lock_guard<std::mutex> lock(cudaHelperMutex_);
    if (fboTexture_) {
        // 注销CUDA互操作（新增）
        if (encoderCudaHelper_) {
            encoderCudaHelper_->release(); // 注销fboCudaResource，置为nullptr
            delete encoderCudaHelper_;     // 析构对象，避免内存泄漏
            encoderCudaHelper_ = nullptr;  // 指针置空，避免悬垂
        }
        glDeleteTextures(1, &fboTexture_);
        fboTexture_ = 0;
    }
    if (fbo_) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
}
