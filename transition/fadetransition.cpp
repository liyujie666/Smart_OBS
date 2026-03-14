#include "fadetransition.h"
#include "cudarenderwidget.h"
#include <QOpenGLExtraFunctions>   // 1. 通用且一定存在
#include <QOpenGLShaderProgram>
FadeTransition::FadeTransition(QObject *parent)
    : TransitionBase(parent)
{
}

void FadeTransition::render(CudaRenderWidget *renderWidget, const QSize &widgetSize,RenderTarget target) {
    if (!m_oldScene || !m_newScene || !renderWidget) return;

    QOpenGLContext *ctx = renderWidget->context();
    if (!ctx || !ctx->isValid()) return;

    QOpenGLExtraFunctions gl(ctx);
    bool isOffscreen = target == RenderTarget::FBO;
    QOpenGLShaderProgram& usedShader = isOffscreen ? renderWidget->getOffscreenShader() : renderWidget->getMainShader();

    // 保存状态（原有逻辑不变，但要适配usedShader）
    GLint oldProg = 0, oldVAO = 0;
    GLint oldSrc = 0, oldDst = 0;
    GLboolean oldBlend = gl.glIsEnabled(GL_BLEND);
    gl.glGetIntegerv(GL_CURRENT_PROGRAM,      &oldProg);
    gl.glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVAO);
    gl.glGetIntegerv(GL_BLEND_SRC_RGB,        &oldSrc);
    gl.glGetIntegerv(GL_BLEND_DST_RGB,        &oldDst);

    // 核心：根据是否离屏，选择对应的渲染接口和VAO
    auto draw = [&](float alpha, Scene *scene) {
        gl.glEnable(GL_BLEND);
        gl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        usedShader.bind();

        // 离屏模式：无需alpha（offscreenShader没alphauniform），窗口模式：传alpha
        usedShader.setUniformValue("alpha", alpha);
        if (!isOffscreen) {
            usedShader.setUniformValue("offset", QVector2D(0,0));
        }

        // 调用对应的渲染接口
        if (isOffscreen) {
            renderWidget->renderSceneLayersOffscreen(scene, widgetSize);
        } else {
            renderWidget->renderSceneLayers(scene, widgetSize);
        }
        usedShader.release();
    };

    if (float a = 1.f - m_progress; a > 0.01f) draw(a, m_oldScene.get());
    if (float a = m_progress; a > 0.01f) draw(a, m_newScene.get());

    // 恢复状态（原有逻辑不变，但要适配usedShader）
    usedShader.bind();
    if (!isOffscreen) {
        usedShader.setUniformValue("alpha",  1.f);
        usedShader.setUniformValue("offset", QVector2D(0,0));
    }
    usedShader.release();

    gl.glUseProgram(oldProg);
    gl.glBindVertexArray(oldVAO);
    if (!oldBlend)
        gl.glDisable(GL_BLEND);
    else
        gl.glBlendFunc(oldSrc, oldDst);

    if (!isOffscreen) renderWidget->update();
}
