#include "slidetransition.h"
#include <QOpenGLExtraFunctions>   // 与 FadeTransition 保持一致
#include <QOpenGLShaderProgram>

SlideTransition::SlideTransition(QObject *parent)
    : TransitionBase{parent}
{}

void SlideTransition::setDirection(Direction direction)
{
    m_direction = direction;
}

void SlideTransition::render(CudaRenderWidget *renderWidget, const QSize &widgetSize, RenderTarget target) {
    if (!m_oldScene || !m_newScene || !renderWidget) return;

    QOpenGLContext *ctx = renderWidget->context();
    if (!ctx || !ctx->isValid()) return;

    QOpenGLExtraFunctions gl(ctx);
    bool isOffscreen = target == RenderTarget::FBO;
    QOpenGLShaderProgram& usedShader = isOffscreen ? renderWidget->getOffscreenShader() : renderWidget->getMainShader();

    // 2. 保存状态（原有逻辑不变）
    GLint oldProg = 0, oldVAO = 0;
    GLint oldBlendSrcRGB = 0, oldBlendDstRGB = 0;
    GLboolean oldBlendEnabled = gl.glIsEnabled(GL_BLEND);
    gl.glGetIntegerv(GL_CURRENT_PROGRAM,&oldProg);
    gl.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    gl.glGetIntegerv(GL_BLEND_SRC_RGB,&oldBlendSrcRGB);
    gl.glGetIntegerv(GL_BLEND_DST_RGB,&oldBlendDstRGB);

    // 3. 偏移量计算：新增精度兜底，解决m_progress未达1.0的残留偏移
    float offsetOld = 0.0f, offsetNew = 0.0f;
    // 转场接近结束时（m_progress≥0.999）强制进度为1.0，确保偏移量完全归零
    float finalProgress = (m_progress >= 0.999f) ? 1.0f : m_progress;

    switch (m_direction) {
    case Left:
        offsetOld = -finalProgress;       // 旧场景：向左滑出（结束时=-1.0，完全不可见）
        offsetNew = 1.0f - finalProgress; // 新场景：从右向左滑入（结束时=0.0，贴合左边界）
        break;
    case Right:
        offsetOld = finalProgress;        // 旧场景：向右滑出（结束时=1.0，完全不可见）
        offsetNew = finalProgress - 1.0f; // 新场景：从左向右滑入（结束时=0.0，贴合右边界）
        break;
    case Up:
        offsetOld = -finalProgress;       // 旧场景：向上滑出（结束时=-1.0，完全不可见）
        offsetNew = 1.0f - finalProgress; // 新场景：从下向上滑入（结束时=0.0，贴合上边界）
        break;
    case Down:
        offsetOld = finalProgress;        // 旧场景：向下滑出（结束时=1.0，完全不可见）
        offsetNew = finalProgress - 1.0f; // 新场景：从上向下滑入（结束时=0.0，贴合下边界）
        break;
    }

    // 4. 渲染场景：优化旧场景渲染条件，避免结束前残留
    auto drawScene = [&](float offset, Scene *scene, bool isOldScene) {
        // 优化：旧场景在finalProgress≥0.99时不渲染（避免遮挡新场景）
        if (isOldScene && finalProgress >= 0.99f) return;

        gl.glEnable(GL_BLEND);
        gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        usedShader.bind();

        // 无论窗口/离屏，都传递offset（offscreenShader已支持offset uniform）
        if (m_direction == Left || m_direction == Right) {
            usedShader.setUniformValue("offset", QVector2D(offset, 0.0f));
        } else {
            usedShader.setUniformValue("offset", QVector2D(0.0f, offset));
        }
        usedShader.setUniformValue("alpha", 1.0f);

        // 调用对应渲染接口
        if (isOffscreen) {
            renderWidget->renderSceneLayersOffscreen(scene, widgetSize);
        } else {
            renderWidget->renderSceneLayers(scene, widgetSize);
        }
        usedShader.release();
    };

    // 5. 渲染旧/新场景：明确标记是否为旧场景，传入drawScene
    drawScene(offsetOld, m_oldScene.get(), true);
    drawScene(offsetNew, m_newScene.get(), false);

    // 6. 恢复状态：关键修改——无论窗口/离屏，都强制设置offset=0，避免残留
    usedShader.bind();
    usedShader.setUniformValue("offset", QVector2D(0.0f, 0.0f));
    usedShader.setUniformValue("alpha",  1.0f);
    usedShader.release();

    gl.glUseProgram(oldProg);
    gl.glBindVertexArray(oldVAO);
    if (!oldBlendEnabled)
        gl.glDisable(GL_BLEND);
    else
        gl.glBlendFunc(oldBlendSrcRGB, oldBlendDstRGB);

    // 窗口模式才需要update（离屏模式由FBO渲染循环触发）
    if (!isOffscreen) renderWidget->update();
}
