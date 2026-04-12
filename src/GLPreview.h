#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QImage>
#include "VideoEffect.h"

class GLPreview : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLPreview(QWidget *parent = nullptr);
    ~GLPreview();

    void displayFrame(const QImage &frame);
    void setDisplayAspectRatio(double aspectRatio);
    void setColorCorrection(const ColorCorrection &cc);
    void setEffectsEnabled(bool enabled) { m_effectsEnabled = enabled; update(); }
    bool effectsEnabled() const { return m_effectsEnabled; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private slots:
    void cleanupGL();

private:
    void createShaderProgram();
    void updateUniforms();

    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLTexture *m_texture = nullptr;
    QOpenGLBuffer m_vbo;
    QOpenGLVertexArrayObject m_vao;

    QImage m_currentFrame;
    ColorCorrection m_cc;
    bool m_effectsEnabled = true;
    bool m_needsUpload = false;
    double m_displayAspectRatio = 0.0;

    // Uniform locations
    int m_locTexture = -1;
    int m_locBrightness = -1;
    int m_locContrast = -1;
    int m_locSaturation = -1;
    int m_locHue = -1;
    int m_locTemperature = -1;
    int m_locTint = -1;
    int m_locGamma = -1;
    int m_locHighlights = -1;
    int m_locShadows = -1;
    int m_locExposure = -1;
    int m_locEffectsEnabled = -1;
};
