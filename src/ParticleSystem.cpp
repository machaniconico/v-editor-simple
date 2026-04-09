#include "ParticleSystem.h"
#include <QPainter>
#include <QRandomGenerator>
#include <QtMath>
#include <algorithm>
#include <cmath>

// --- Helpers ---

static double randDouble(double min, double max)
{
    if (min >= max) return min;
    return min + QRandomGenerator::global()->generateDouble() * (max - min);
}

static double degToRad(double deg)
{
    return deg * M_PI / 180.0;
}

// --- ParticleSystem ---

ParticleSystem::ParticleSystem() = default;

void ParticleSystem::setConfig(const ParticleEmitterConfig &config)
{
    m_config = config;
}

void ParticleSystem::reset()
{
    m_particles.clear();
    m_emitAccumulator = 0.0;
}

// --- Simulation ---

void ParticleSystem::update(double deltaTime)
{
    if (deltaTime <= 0.0) return;

    // Update existing particles
    for (int i = m_particles.size() - 1; i >= 0; --i) {
        Particle &p = m_particles[i];

        // Apply forces
        p.velocity += p.acceleration * deltaTime;
        p.velocity += m_config.gravity * deltaTime;
        p.velocity += m_config.wind * deltaTime;

        // Integrate position
        p.position += p.velocity * deltaTime;

        // Rotation
        p.rotation += p.rotationSpeed * deltaTime;

        // Age
        p.life -= deltaTime;

        // Remove dead particles
        if (p.life <= 0.0) {
            m_particles.removeAt(i);
        }
    }

    // Spawn new particles
    m_emitAccumulator += m_config.emitRate * deltaTime;
    int toSpawn = static_cast<int>(m_emitAccumulator);
    m_emitAccumulator -= toSpawn;

    for (int i = 0; i < toSpawn && m_particles.size() < m_config.maxParticles; ++i) {
        spawnParticle(QSize(1, 1)); // canvas size set during render
    }
}

void ParticleSystem::spawnParticle(const QSize &canvasSize)
{
    Particle p;

    // Position within emission area (normalized coords * canvas)
    double cx = m_config.emitPosition.x();
    double cy = m_config.emitPosition.y();
    double hw = m_config.emitAreaSize.width() * 0.5;
    double hh = m_config.emitAreaSize.height() * 0.5;

    double w = canvasSize.width() > 1 ? canvasSize.width() : 1920.0;
    double h = canvasSize.height() > 1 ? canvasSize.height() : 1080.0;

    p.position.setX((cx + randDouble(-hw, hw)) * w);
    p.position.setY((cy + randDouble(-hh, hh)) * h);

    // Velocity from direction + spread
    double angle = degToRad(m_config.direction + randDouble(-m_config.spread, m_config.spread));
    double speed = randDouble(m_config.speedMin, m_config.speedMax);
    p.velocity = QPointF(std::cos(angle) * speed, std::sin(angle) * speed);

    p.acceleration = QPointF(0.0, 0.0);

    // Life
    p.maxLife = randDouble(m_config.lifeMin, m_config.lifeMax);
    p.life = p.maxLife;

    // Size
    p.size = randDouble(m_config.sizeMin, m_config.sizeMax);

    // Rotation
    p.rotation = randDouble(0.0, 360.0);
    p.rotationSpeed = 0.0;
    if (m_config.type == ParticleType::Confetti)
        p.rotationSpeed = randDouble(-360.0, 360.0);

    // Color — slight randomization for natural look
    p.color = m_config.startColor;
    if (m_config.type == ParticleType::Confetti) {
        // Random hue for confetti
        int hue = QRandomGenerator::global()->bounded(360);
        p.color = QColor::fromHsv(hue, 220, 255);
    }

    p.opacity = 1.0;

    m_particles.append(p);
}

// --- Per-particle helpers ---

double ParticleSystem::lifeFraction(const Particle &p)
{
    if (p.maxLife <= 0.0) return 1.0;
    return 1.0 - (p.life / p.maxLife); // 0 = just born, 1 = about to die
}

double ParticleSystem::computeOpacity(const Particle &p, const ParticleEmitterConfig &cfg)
{
    double t = lifeFraction(p);
    double opacity = 1.0;

    // Fade in
    if (cfg.fadeIn > 0.0 && t < cfg.fadeIn)
        opacity = t / cfg.fadeIn;

    // Fade out
    if (cfg.fadeOut > 0.0 && t > (1.0 - cfg.fadeOut))
        opacity = (1.0 - t) / cfg.fadeOut;

    return qBound(0.0, opacity, 1.0);
}

QColor ParticleSystem::interpolateColor(const QColor &a, const QColor &b, double t)
{
    t = qBound(0.0, t, 1.0);
    return QColor(
        static_cast<int>(a.red()   + (b.red()   - a.red())   * t),
        static_cast<int>(a.green() + (b.green() - a.green()) * t),
        static_cast<int>(a.blue()  + (b.blue()  - a.blue())  * t),
        static_cast<int>(a.alpha() + (b.alpha() - a.alpha()) * t)
    );
}

double ParticleSystem::computeSize(const Particle &p, const ParticleEmitterConfig &cfg)
{
    double t = lifeFraction(p);
    double mult = cfg.sizeStartMult + (cfg.sizeEndMult - cfg.sizeStartMult) * t;
    return p.size * mult;
}

// --- Rendering ---

void ParticleSystem::renderParticle(QPainter &painter, const Particle &p,
                                     const ParticleEmitterConfig &cfg) const
{
    double t = lifeFraction(p);
    double opacity = computeOpacity(p, cfg);
    double size = computeSize(p, cfg);
    QColor color = interpolateColor(cfg.startColor, cfg.endColor, t);

    // Override color for confetti (per-particle hue)
    if (cfg.type == ParticleType::Confetti)
        color = p.color;

    color.setAlphaF(opacity);

    painter.save();
    painter.translate(p.position);
    painter.rotate(p.rotation);

    // Set composition mode for glowing effects
    if (cfg.type == ParticleType::Spark || cfg.type == ParticleType::Fire)
        painter.setCompositionMode(QPainter::CompositionMode_Plus);
    else
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);

    if (cfg.type == ParticleType::Rain) {
        // Rain: draw as short line streaks
        QPen rainPen(color, qMax(1.0, size * 0.3));
        rainPen.setCapStyle(Qt::RoundCap);
        painter.setPen(rainPen);
        painter.setBrush(Qt::NoBrush);
        double length = size * 3.0;
        painter.drawLine(QPointF(0, -length * 0.5), QPointF(0, length * 0.5));
    } else if (cfg.type == ParticleType::Star) {
        // Star: draw a small 4-pointed star shape
        double r = size * 0.5;
        QPolygonF star;
        for (int i = 0; i < 8; ++i) {
            double a = i * M_PI / 4.0;
            double rad = (i % 2 == 0) ? r : r * 0.4;
            star << QPointF(std::cos(a) * rad, std::sin(a) * rad);
        }
        painter.drawPolygon(star);
    } else {
        // Default: filled circle/ellipse
        double r = size * 0.5;
        painter.drawEllipse(QPointF(0, 0), r, r);
    }

    painter.restore();
}

QImage ParticleSystem::renderFrame(const QSize &canvasSize, double time)
{
    Q_UNUSED(time);

    QImage image(canvasSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    for (const Particle &p : m_particles)
        renderParticle(painter, p, m_config);

    painter.end();
    return image;
}

QVector<QImage> ParticleSystem::renderParticleSequence(const QSize &canvasSize,
                                                        double startTime, double endTime,
                                                        double fps)
{
    QVector<QImage> frames;
    if (fps <= 0.0 || endTime <= startTime) return frames;

    double dt = 1.0 / fps;
    int frameCount = static_cast<int>((endTime - startTime) * fps);
    frames.reserve(frameCount);

    // Reset and pre-simulate to startTime if needed
    reset();

    // Use a smaller step for pre-warming (simulate from 0 to startTime)
    if (startTime > 0.0) {
        double preWarmStep = 1.0 / 30.0; // 30 fps resolution for pre-warm
        double preWarmTime = 0.0;
        while (preWarmTime < startTime) {
            double step = qMin(preWarmStep, startTime - preWarmTime);
            // Temporarily set canvas size for spawn positions
            m_emitAccumulator += m_config.emitRate * step;
            int toSpawn = static_cast<int>(m_emitAccumulator);
            m_emitAccumulator -= toSpawn;
            for (int i = 0; i < toSpawn && m_particles.size() < m_config.maxParticles; ++i)
                spawnParticle(canvasSize);
            // Update physics without spawning again
            for (int i = m_particles.size() - 1; i >= 0; --i) {
                Particle &p = m_particles[i];
                p.velocity += p.acceleration * step;
                p.velocity += m_config.gravity * step;
                p.velocity += m_config.wind * step;
                p.position += p.velocity * step;
                p.rotation += p.rotationSpeed * step;
                p.life -= step;
                if (p.life <= 0.0)
                    m_particles.removeAt(i);
            }
            preWarmTime += step;
        }
    }

    // Render each frame
    for (int f = 0; f < frameCount; ++f) {
        double currentTime = startTime + f * dt;

        // Spawn new particles for this step
        m_emitAccumulator += m_config.emitRate * dt;
        int toSpawn = static_cast<int>(m_emitAccumulator);
        m_emitAccumulator -= toSpawn;
        for (int i = 0; i < toSpawn && m_particles.size() < m_config.maxParticles; ++i)
            spawnParticle(canvasSize);

        // Update physics
        for (int i = m_particles.size() - 1; i >= 0; --i) {
            Particle &p = m_particles[i];
            p.velocity += p.acceleration * dt;
            p.velocity += m_config.gravity * dt;
            p.velocity += m_config.wind * dt;
            p.position += p.velocity * dt;
            p.rotation += p.rotationSpeed * dt;
            p.life -= dt;
            if (p.life <= 0.0)
                m_particles.removeAt(i);
        }

        frames.append(renderFrame(canvasSize, currentTime));
    }

    return frames;
}

// --- Presets ---

QMap<QString, ParticleEmitterConfig> ParticleSystem::presetConfigs()
{
    QMap<QString, ParticleEmitterConfig> presets;

    // Snow — white, slow fall, slight wind, medium size
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Snow;
        c.emitRate = 40.0;
        c.maxParticles = 400;
        c.emitPosition = QPointF(0.5, 0.0);
        c.emitAreaSize = QSizeF(1.0, 0.0);
        c.lifeMin = 4.0;
        c.lifeMax = 7.0;
        c.sizeMin = 3.0;
        c.sizeMax = 8.0;
        c.speedMin = 20.0;
        c.speedMax = 50.0;
        c.direction = 90.0;   // downward (90 = +Y in screen coords)
        c.spread = 20.0;
        c.gravity = QPointF(0.0, 10.0);
        c.wind = QPointF(15.0, 0.0);
        c.startColor = QColor(255, 255, 255, 220);
        c.endColor = QColor(200, 220, 255, 180);
        c.fadeIn = 0.1;
        c.fadeOut = 0.3;
        c.sizeStartMult = 0.8;
        c.sizeEndMult = 1.0;
        presets["Snow"] = c;
    }

    // Rain — blue-gray, fast downward, thin streaks
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Rain;
        c.emitRate = 120.0;
        c.maxParticles = 800;
        c.emitPosition = QPointF(0.5, 0.0);
        c.emitAreaSize = QSizeF(1.2, 0.0);
        c.lifeMin = 0.8;
        c.lifeMax = 1.5;
        c.sizeMin = 1.0;
        c.sizeMax = 2.5;
        c.speedMin = 300.0;
        c.speedMax = 500.0;
        c.direction = 95.0;   // slightly angled rain
        c.spread = 5.0;
        c.gravity = QPointF(0.0, 200.0);
        c.wind = QPointF(30.0, 0.0);
        c.startColor = QColor(180, 200, 220, 180);
        c.endColor = QColor(150, 170, 200, 120);
        c.fadeIn = 0.0;
        c.fadeOut = 0.1;
        c.sizeStartMult = 1.0;
        c.sizeEndMult = 1.0;
        presets["Rain"] = c;
    }

    // Sparks — orange-yellow, upward burst, gravity pull down, glow
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Spark;
        c.emitRate = 60.0;
        c.maxParticles = 300;
        c.emitPosition = QPointF(0.5, 0.8);
        c.emitAreaSize = QSizeF(0.1, 0.0);
        c.lifeMin = 0.5;
        c.lifeMax = 1.5;
        c.sizeMin = 2.0;
        c.sizeMax = 5.0;
        c.speedMin = 100.0;
        c.speedMax = 250.0;
        c.direction = 270.0;  // upward
        c.spread = 40.0;
        c.gravity = QPointF(0.0, 150.0);
        c.wind = QPointF(0.0, 0.0);
        c.startColor = QColor(255, 200, 50);
        c.endColor = QColor(255, 100, 20);
        c.fadeIn = 0.0;
        c.fadeOut = 0.4;
        c.sizeStartMult = 1.0;
        c.sizeEndMult = 0.3;
        presets["Sparks"] = c;
    }

    // Smoke — gray, slow rise, grow over life, fade out
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Smoke;
        c.emitRate = 20.0;
        c.maxParticles = 200;
        c.emitPosition = QPointF(0.5, 0.8);
        c.emitAreaSize = QSizeF(0.05, 0.0);
        c.lifeMin = 3.0;
        c.lifeMax = 5.0;
        c.sizeMin = 8.0;
        c.sizeMax = 15.0;
        c.speedMin = 15.0;
        c.speedMax = 40.0;
        c.direction = 270.0;  // upward
        c.spread = 15.0;
        c.gravity = QPointF(0.0, -5.0);
        c.wind = QPointF(10.0, 0.0);
        c.startColor = QColor(120, 120, 120, 160);
        c.endColor = QColor(80, 80, 80, 40);
        c.fadeIn = 0.1;
        c.fadeOut = 0.5;
        c.sizeStartMult = 0.5;
        c.sizeEndMult = 2.5;
        presets["Smoke"] = c;
    }

    // Fire — red to orange to yellow, upward, shrink over life
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Fire;
        c.emitRate = 80.0;
        c.maxParticles = 400;
        c.emitPosition = QPointF(0.5, 0.85);
        c.emitAreaSize = QSizeF(0.08, 0.0);
        c.lifeMin = 0.5;
        c.lifeMax = 1.2;
        c.sizeMin = 5.0;
        c.sizeMax = 12.0;
        c.speedMin = 40.0;
        c.speedMax = 100.0;
        c.direction = 270.0;  // upward
        c.spread = 25.0;
        c.gravity = QPointF(0.0, -20.0);
        c.wind = QPointF(5.0, 0.0);
        c.startColor = QColor(255, 80, 20);
        c.endColor = QColor(255, 220, 50);
        c.fadeIn = 0.0;
        c.fadeOut = 0.3;
        c.sizeStartMult = 1.2;
        c.sizeEndMult = 0.2;
        presets["Fire"] = c;
    }

    // Confetti — multi-color, random rotation, gravity
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Confetti;
        c.emitRate = 50.0;
        c.maxParticles = 500;
        c.emitPosition = QPointF(0.5, 0.1);
        c.emitAreaSize = QSizeF(0.8, 0.0);
        c.lifeMin = 3.0;
        c.lifeMax = 6.0;
        c.sizeMin = 4.0;
        c.sizeMax = 8.0;
        c.speedMin = 20.0;
        c.speedMax = 60.0;
        c.direction = 90.0;   // downward
        c.spread = 60.0;
        c.gravity = QPointF(0.0, 40.0);
        c.wind = QPointF(10.0, 0.0);
        c.startColor = Qt::white; // overridden per-particle
        c.endColor = Qt::white;
        c.fadeIn = 0.0;
        c.fadeOut = 0.2;
        c.sizeStartMult = 1.0;
        c.sizeEndMult = 1.0;
        presets["Confetti"] = c;
    }

    // Dust — brown, slow float, tiny size
    {
        ParticleEmitterConfig c;
        c.type = ParticleType::Dust;
        c.emitRate = 15.0;
        c.maxParticles = 150;
        c.emitPosition = QPointF(0.5, 0.5);
        c.emitAreaSize = QSizeF(1.0, 1.0);
        c.lifeMin = 4.0;
        c.lifeMax = 8.0;
        c.sizeMin = 1.0;
        c.sizeMax = 3.0;
        c.speedMin = 5.0;
        c.speedMax = 15.0;
        c.direction = 90.0;
        c.spread = 180.0;     // omnidirectional float
        c.gravity = QPointF(0.0, 2.0);
        c.wind = QPointF(3.0, 0.0);
        c.startColor = QColor(160, 130, 90, 120);
        c.endColor = QColor(140, 110, 70, 60);
        c.fadeIn = 0.2;
        c.fadeOut = 0.4;
        c.sizeStartMult = 0.8;
        c.sizeEndMult = 1.2;
        presets["Dust"] = c;
    }

    return presets;
}
