#include "GLPreview.h"
#include <cmath>
#include <QtGlobal>
#include <QOpenGLContext>
#include <QDebug>

// Vertex shader — simple fullscreen quad
static const char *vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

// Fragment shader — color correction + color grading pipeline on GPU
static const char *fragmentShaderSrc = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform bool uEffectsEnabled;

// Color correction uniforms
uniform float uBrightness;   // -100 to 100
uniform float uContrast;     // -100 to 100
uniform float uSaturation;   // -100 to 100
uniform float uHue;          // -180 to 180
uniform float uTemperature;  // -100 to 100
uniform float uTint;         // -100 to 100
uniform float uGamma;        // 0.1 to 3.0
uniform float uHighlights;   // -100 to 100
uniform float uShadows;      // -100 to 100
uniform float uExposure;     // -3.0 to 3.0

// Lift/Gamma/Gain color wheels (DaVinci Resolve style)
uniform float uLiftR, uLiftG, uLiftB;     // -1.0 to 1.0
uniform float uGammaR, uGammaG, uGammaB;  // -1.0 to 1.0
uniform float uGainR, uGainG, uGainB;     // -1.0 to 1.0

// 3D LUT uniforms
uniform sampler3D uLut3D;
uniform float uLutIntensity;  // 0.0 to 1.0
uniform bool uLutEnabled;

vec3 adjustExposure(vec3 color, float exposure) {
    return color * pow(2.0, exposure);
}

vec3 adjustBrightnessContrast(vec3 color, float brightness, float contrast) {
    vec3 c = color + brightness / 100.0;
    float factor = (100.0 + contrast) / 100.0;
    factor *= factor;
    c = (c - 0.5) * factor + 0.5;
    return c;
}

vec3 adjustHighlightsShadows(vec3 color, float highlights, float shadows) {
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float hWeight = lum * lum;
    float sWeight = (1.0 - lum) * (1.0 - lum);
    float adjust = highlights / 100.0 * hWeight + shadows / 100.0 * sWeight;
    return color + adjust;
}

vec3 adjustSaturation(vec3 color, float saturation) {
    float factor = (saturation + 100.0) / 100.0;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(lum), color, factor);
}

vec3 adjustHue(vec3 color, float hue) {
    float angle = radians(hue);
    float cosA = cos(angle);
    float sinA = sin(angle);

    mat3 hueRotation = mat3(
        0.213 + 0.787 * cosA - 0.213 * sinA,
        0.213 - 0.213 * cosA + 0.143 * sinA,
        0.213 - 0.213 * cosA - 0.787 * sinA,
        0.715 - 0.715 * cosA - 0.715 * sinA,
        0.715 + 0.285 * cosA + 0.140 * sinA,
        0.715 - 0.715 * cosA + 0.715 * sinA,
        0.072 - 0.072 * cosA + 0.928 * sinA,
        0.072 - 0.072 * cosA - 0.283 * sinA,
        0.072 + 0.928 * cosA + 0.072 * sinA
    );

    return hueRotation * color;
}

vec3 adjustTemperatureTint(vec3 color, float temperature, float tint) {
    float rShift = temperature * 0.005;
    float bShift = -temperature * 0.005;
    float gShift = -tint * 0.003;
    float mShift = tint * 0.002;
    color.r += rShift + mShift;
    color.g += gShift;
    color.b += bShift + mShift;
    return color;
}

vec3 adjustGamma(vec3 color, float gamma) {
    float invGamma = 1.0 / gamma;
    return pow(max(color, vec3(0.0)), vec3(invGamma));
}

// DaVinci Resolve-style Lift/Gamma/Gain
// Lift  = shadows offset (applied more in darks)
// Gamma = midtone power (applied via power function weighted to midtones)
// Gain  = highlight multiplier (scales the entire signal)
vec3 applyLiftGammaGain(vec3 color, vec3 lift, vec3 gamma, vec3 gain) {
    // Gain: multiply signal  (1.0 + gain adjustment)
    vec3 gained = color * (vec3(1.0) + gain);

    // Lift: add offset weighted by inverse luminance (affects shadows more)
    vec3 lifted = gained + lift * (vec3(1.0) - gained);

    // Gamma: power curve through midtones
    // gamma adjustment maps [-1,1] to a power curve: 0 = no change, negative = darken mids, positive = brighten mids
    vec3 gammaPow = vec3(1.0) / max(vec3(1.0) + gamma, vec3(0.01));
    vec3 result = pow(max(lifted, vec3(0.0)), gammaPow);

    return result;
}

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    vec3 color = texColor.rgb;

    if (uEffectsEnabled) {
        // Apply color correction pipeline (same order as CPU)
        if (uExposure != 0.0)
            color = adjustExposure(color, uExposure);
        if (uBrightness != 0.0 || uContrast != 0.0)
            color = adjustBrightnessContrast(color, uBrightness, uContrast);
        if (uHighlights != 0.0 || uShadows != 0.0)
            color = adjustHighlightsShadows(color, uHighlights, uShadows);
        if (uSaturation != 0.0)
            color = adjustSaturation(color, uSaturation);
        if (uHue != 0.0)
            color = adjustHue(color, uHue);
        if (uTemperature != 0.0 || uTint != 0.0)
            color = adjustTemperatureTint(color, uTemperature, uTint);
        if (uGamma != 1.0)
            color = adjustGamma(color, uGamma);

        // Lift/Gamma/Gain color wheels (DaVinci Resolve style)
        vec3 lift  = vec3(uLiftR,  uLiftG,  uLiftB);
        vec3 gamma = vec3(uGammaR, uGammaG, uGammaB);
        vec3 gain  = vec3(uGainR,  uGainG,  uGainB);
        if (lift != vec3(0.0) || gamma != vec3(0.0) || gain != vec3(0.0))
            color = applyLiftGammaGain(color, lift, gamma, gain);

        // 3D LUT application
        if (uLutEnabled) {
            vec3 lutColor = texture(uLut3D, clamp(color, 0.0, 1.0)).rgb;
            color = mix(color, lutColor, uLutIntensity);
        }
    }

    FragColor = vec4(clamp(color, 0.0, 1.0), texColor.a);
}
)";

GLPreview::GLPreview(QWidget *parent)
    : QOpenGLWidget(parent), m_vbo(QOpenGLBuffer::VertexBuffer)
{
    setMinimumSize(320, 180);
}

GLPreview::~GLPreview()
{
    // Primary cleanup path is cleanupGL() via QOpenGLContext::aboutToBeDestroyed.
    // Fallback: only touch GL state here if a current context is available — calling
    // makeCurrent() on a destroyed context segfaults some drivers on shutdown.
    if (QOpenGLContext::currentContext() || context()) {
        cleanupGL();
    } else {
        // Context already gone — clear raw pointers to skip double-free, but don't
        // touch GL state.
        m_texture = nullptr;
        m_program = nullptr;
    }
}

void GLPreview::cleanupGL()
{
    if (!context())
        return;

    makeCurrent();
    if (m_lutTexture) {
        delete m_lutTexture;
        m_lutTexture = nullptr;
    }
    if (m_texture) {
        delete m_texture;
        m_texture = nullptr;
    }
    if (m_program) {
        delete m_program;
        m_program = nullptr;
    }
    if (m_vbo.isCreated())
        m_vbo.destroy();
    if (m_vao.isCreated())
        m_vao.destroy();
    doneCurrent();
}

void GLPreview::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    // Qt recommends cleaning up GL resources when the context is about to be
    // destroyed rather than in the widget destructor (Qt 5/6 QOpenGLWidget docs).
    if (auto *ctx = context()) {
        connect(ctx, &QOpenGLContext::aboutToBeDestroyed,
                this, &GLPreview::cleanupGL, Qt::UniqueConnection);
    }

    createShaderProgram();

    // Fullscreen quad: position(x,y) + texcoord(u,v)
    float vertices[] = {
        // pos        // tex
        -1.0f,  1.0f,  0.0f, 0.0f,  // top-left
        -1.0f, -1.0f,  0.0f, 1.0f,  // bottom-left
         1.0f,  1.0f,  1.0f, 0.0f,  // top-right
         1.0f, -1.0f,  1.0f, 1.0f,  // bottom-right
    };

    m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(vertices, sizeof(vertices));

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    // TexCoord attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));

    m_vbo.release();
    m_vao.release();
}

void GLPreview::createShaderProgram()
{
    m_program = new QOpenGLShaderProgram(this);
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSrc)) {
        qWarning() << "GLPreview: vertex shader compile failed:" << m_program->log();
    }
    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSrc)) {
        qWarning() << "GLPreview: fragment shader compile failed:" << m_program->log();
    }
    if (!m_program->link()) {
        qWarning() << "GLPreview: shader link failed:" << m_program->log();
        delete m_program;
        m_program = nullptr;
        return;
    }

    m_locTexture        = m_program->uniformLocation("uTexture");
    m_locBrightness     = m_program->uniformLocation("uBrightness");
    m_locContrast       = m_program->uniformLocation("uContrast");
    m_locSaturation     = m_program->uniformLocation("uSaturation");
    m_locHue            = m_program->uniformLocation("uHue");
    m_locTemperature    = m_program->uniformLocation("uTemperature");
    m_locTint           = m_program->uniformLocation("uTint");
    m_locGamma          = m_program->uniformLocation("uGamma");
    m_locHighlights     = m_program->uniformLocation("uHighlights");
    m_locShadows        = m_program->uniformLocation("uShadows");
    m_locExposure       = m_program->uniformLocation("uExposure");
    m_locEffectsEnabled = m_program->uniformLocation("uEffectsEnabled");

    // Lift/Gamma/Gain
    m_locLiftR  = m_program->uniformLocation("uLiftR");
    m_locLiftG  = m_program->uniformLocation("uLiftG");
    m_locLiftB  = m_program->uniformLocation("uLiftB");
    m_locGammaR = m_program->uniformLocation("uGammaR");
    m_locGammaG = m_program->uniformLocation("uGammaG");
    m_locGammaB = m_program->uniformLocation("uGammaB");
    m_locGainR  = m_program->uniformLocation("uGainR");
    m_locGainG  = m_program->uniformLocation("uGainG");
    m_locGainB  = m_program->uniformLocation("uGainB");

    // LUT
    m_locLut3D         = m_program->uniformLocation("uLut3D");
    m_locLutIntensity  = m_program->uniformLocation("uLutIntensity");
    m_locLutEnabled    = m_program->uniformLocation("uLutEnabled");
}

void GLPreview::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void GLPreview::displayFrame(const QImage &frame)
{
    if (frame.isNull()) {
        qWarning() << "GLPreview::displayFrame called with null image";
        return;
    }
    m_currentFrame = frame.convertToFormat(QImage::Format_RGBA8888);
    if (m_currentFrame.isNull()) {
        qWarning() << "GLPreview: convertToFormat returned null";
        return;
    }
    if (m_displayAspectRatio <= 0.0 && m_currentFrame.height() > 0)
        m_displayAspectRatio = static_cast<double>(m_currentFrame.width()) / m_currentFrame.height();
    m_needsUpload = true;
    update();
}

void GLPreview::setDisplayAspectRatio(double aspectRatio)
{
    m_displayAspectRatio = (aspectRatio > 0.0) ? aspectRatio : 0.0;
    update();
}

void GLPreview::setColorCorrection(const ColorCorrection &cc)
{
    m_cc = cc;
    update();
}

void GLPreview::paintGL()
{
    static int paintCount = 0;
    if (++paintCount <= 5 || (paintCount % 100) == 0) {
        qInfo() << "GLPreview::paintGL #" << paintCount
                << "widget(logical)=" << width() << "x" << height()
                << "dpr=" << devicePixelRatioF()
                << "frame=" << m_currentFrame.width() << "x" << m_currentFrame.height()
                << "upload=" << m_needsUpload;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (m_currentFrame.isNull()) return;

    // glViewport expects PHYSICAL pixels, but QWidget::width()/height() are
    // LOGICAL (device-independent) pixels. On a high-DPI display with DPR=1.5
    // or 2.0, using logical coordinates makes the video render in a fraction
    // of the widget — which is what the "small video in big panel" bug was.
    const qreal dpr = devicePixelRatioF();
    const int physW = qMax(1, qRound(width() * dpr));
    const int physH = qMax(1, qRound(height() * dpr));

    const double frameAspect =
        (m_displayAspectRatio > 0.0 && std::isfinite(m_displayAspectRatio))
            ? m_displayAspectRatio
            : ((m_currentFrame.height() > 0)
                   ? static_cast<double>(m_currentFrame.width()) / m_currentFrame.height()
                   : 1.0);
    const double widgetAspect =
        (physH > 0) ? static_cast<double>(physW) / physH : frameAspect;

    int viewportX = 0;
    int viewportY = 0;
    int viewportW = physW;
    int viewportH = physH;

    if (frameAspect > 0.0 && widgetAspect > 0.0) {
        if (widgetAspect > frameAspect) {
            viewportW = qMax(1, qRound(physH * frameAspect));
            viewportX = (physW - viewportW) / 2;
        } else {
            viewportH = qMax(1, qRound(physW / frameAspect));
            viewportY = (physH - viewportH) / 2;
        }
    }

    // Upload texture if new frame.
    //
    // Re-use a single QOpenGLTexture across frames — allocating a new texture
    // per frame (~8 MB for 1080p RGBA) thrashes driver memory and has been
    // observed to crash Intel/AMD drivers after a few hundred frames.
    if (m_needsUpload) {
        const int fw = m_currentFrame.width();
        const int fh = m_currentFrame.height();
        if (fw <= 0 || fh <= 0) {
            m_needsUpload = false;
            return;
        }

        const bool sizeChanged = !m_texture
            || m_texture->width() != fw
            || m_texture->height() != fh;

        if (sizeChanged) {
            if (m_texture) {
                delete m_texture;
                m_texture = nullptr;
            }
            m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
            m_texture->setSize(fw, fh);
            m_texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
            m_texture->setMinificationFilter(QOpenGLTexture::Linear);
            m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
            m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
            m_texture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        }

        // Upload pixel data into the already-allocated texture.
        m_texture->setData(0, 0,
                           QOpenGLTexture::RGBA, QOpenGLTexture::UInt8,
                           static_cast<const void*>(m_currentFrame.constBits()));
        m_needsUpload = false;
    }

    if (!m_texture || !m_program) return;

    m_program->bind();
    m_texture->bind();
    glViewport(viewportX, viewportY, viewportW, viewportH);

    // Set uniforms
    m_program->setUniformValue(m_locTexture, 0);
    m_program->setUniformValue(m_locEffectsEnabled, m_effectsEnabled);
    m_program->setUniformValue(m_locBrightness,  static_cast<float>(m_cc.brightness));
    m_program->setUniformValue(m_locContrast,    static_cast<float>(m_cc.contrast));
    m_program->setUniformValue(m_locSaturation,  static_cast<float>(m_cc.saturation));
    m_program->setUniformValue(m_locHue,         static_cast<float>(m_cc.hue));
    m_program->setUniformValue(m_locTemperature, static_cast<float>(m_cc.temperature));
    m_program->setUniformValue(m_locTint,        static_cast<float>(m_cc.tint));
    m_program->setUniformValue(m_locGamma,       static_cast<float>(m_cc.gamma));
    m_program->setUniformValue(m_locHighlights,  static_cast<float>(m_cc.highlights));
    m_program->setUniformValue(m_locShadows,     static_cast<float>(m_cc.shadows));
    m_program->setUniformValue(m_locExposure,    static_cast<float>(m_cc.exposure));

    // Lift/Gamma/Gain
    m_program->setUniformValue(m_locLiftR,  static_cast<float>(m_cc.liftR));
    m_program->setUniformValue(m_locLiftG,  static_cast<float>(m_cc.liftG));
    m_program->setUniformValue(m_locLiftB,  static_cast<float>(m_cc.liftB));
    m_program->setUniformValue(m_locGammaR, static_cast<float>(m_cc.gammaR));
    m_program->setUniformValue(m_locGammaG, static_cast<float>(m_cc.gammaG));
    m_program->setUniformValue(m_locGammaB, static_cast<float>(m_cc.gammaB));
    m_program->setUniformValue(m_locGainR,  static_cast<float>(m_cc.gainR));
    m_program->setUniformValue(m_locGainG,  static_cast<float>(m_cc.gainG));
    m_program->setUniformValue(m_locGainB,  static_cast<float>(m_cc.gainB));

    // LUT
    m_program->setUniformValue(m_locLutEnabled, m_lutEnabled);
    m_program->setUniformValue(m_locLutIntensity, m_lutIntensity);
    if (m_lutEnabled && m_lutTexture) {
        glActiveTexture(GL_TEXTURE1);
        m_lutTexture->bind();
        m_program->setUniformValue(m_locLut3D, 1);
    }

    // Draw quad
    m_vao.bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    m_vao.release();

    if (m_lutEnabled && m_lutTexture) {
        glActiveTexture(GL_TEXTURE1);
        m_lutTexture->release();
        glActiveTexture(GL_TEXTURE0);
    }

    m_texture->release();
    m_program->release();
    glViewport(0, 0, physW, physH);
}

void GLPreview::setLut(const LutData &lut)
{
    if (!lut.isValid()) {
        clearLut();
        return;
    }

    makeCurrent();

    if (m_lutTexture) {
        delete m_lutTexture;
        m_lutTexture = nullptr;
    }

    // Create 3D texture from LUT table
    m_lutTexture = new QOpenGLTexture(QOpenGLTexture::Target3D);
    m_lutTexture->setSize(lut.size, lut.size, lut.size);
    m_lutTexture->setFormat(QOpenGLTexture::RGB32F);
    m_lutTexture->allocateStorage();
    m_lutTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_lutTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_lutTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    // Upload LUT data as float RGB
    QVector<float> data;
    data.reserve(lut.table.size() * 3);
    for (const QVector3D &v : lut.table) {
        data.append(v.x());
        data.append(v.y());
        data.append(v.z());
    }
    m_lutTexture->setData(QOpenGLTexture::RGB, QOpenGLTexture::Float32,
                          data.constData());

    m_lutIntensity = static_cast<float>(lut.intensity);
    m_lutEnabled = true;

    doneCurrent();
    update();
}

void GLPreview::clearLut()
{
    makeCurrent();
    if (m_lutTexture) {
        delete m_lutTexture;
        m_lutTexture = nullptr;
    }
    m_lutEnabled = false;
    m_lutIntensity = 1.0f;
    doneCurrent();
    update();
}
