#include "TrackerLink.h"

#include <QJsonDocument>
#include <QtMath>
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Helper: enum <-> string conversion
// ---------------------------------------------------------------------------

static QString linkTargetToString(LinkTarget t)
{
    switch (t) {
    case LinkTarget::TextOverlay:     return QStringLiteral("TextOverlay");
    case LinkTarget::ImageOverlay:    return QStringLiteral("ImageOverlay");
    case LinkTarget::Effect:          return QStringLiteral("Effect");
    case LinkTarget::Mask:            return QStringLiteral("Mask");
    case LinkTarget::ShapeLayer:      return QStringLiteral("ShapeLayer");
    case LinkTarget::ParticleEmitter: return QStringLiteral("ParticleEmitter");
    }
    return QStringLiteral("TextOverlay");
}

static LinkTarget linkTargetFromString(const QString &s)
{
    if (s == QLatin1String("ImageOverlay"))    return LinkTarget::ImageOverlay;
    if (s == QLatin1String("Effect"))          return LinkTarget::Effect;
    if (s == QLatin1String("Mask"))            return LinkTarget::Mask;
    if (s == QLatin1String("ShapeLayer"))      return LinkTarget::ShapeLayer;
    if (s == QLatin1String("ParticleEmitter")) return LinkTarget::ParticleEmitter;
    return LinkTarget::TextOverlay;
}

static QString linkPropertyToString(LinkProperty p)
{
    switch (p) {
    case LinkProperty::PositionX: return QStringLiteral("PositionX");
    case LinkProperty::PositionY: return QStringLiteral("PositionY");
    case LinkProperty::ScaleX:    return QStringLiteral("ScaleX");
    case LinkProperty::ScaleY:    return QStringLiteral("ScaleY");
    case LinkProperty::Rotation:  return QStringLiteral("Rotation");
    case LinkProperty::Opacity:   return QStringLiteral("Opacity");
    }
    return QStringLiteral("PositionX");
}

static LinkProperty linkPropertyFromString(const QString &s)
{
    if (s == QLatin1String("PositionY")) return LinkProperty::PositionY;
    if (s == QLatin1String("ScaleX"))    return LinkProperty::ScaleX;
    if (s == QLatin1String("ScaleY"))    return LinkProperty::ScaleY;
    if (s == QLatin1String("Rotation"))  return LinkProperty::Rotation;
    if (s == QLatin1String("Opacity"))   return LinkProperty::Opacity;
    return LinkProperty::PositionX;
}

// ---------------------------------------------------------------------------
// TrackerLinkConfig serialisation
// ---------------------------------------------------------------------------

QJsonObject TrackerLinkConfig::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("targetType")]     = linkTargetToString(targetType);
    obj[QStringLiteral("targetIndex")]    = targetIndex;
    obj[QStringLiteral("property")]       = linkPropertyToString(property);
    obj[QStringLiteral("sourceTrackIndex")] = sourceTrackIndex;
    obj[QStringLiteral("offsetX")]        = offset.x();
    obj[QStringLiteral("offsetY")]        = offset.y();
    obj[QStringLiteral("smoothing")]      = smoothing;
    obj[QStringLiteral("delay")]          = delay;
    obj[QStringLiteral("invertX")]        = invertX;
    obj[QStringLiteral("invertY")]        = invertY;
    obj[QStringLiteral("scale")]          = scale;
    return obj;
}

TrackerLinkConfig TrackerLinkConfig::fromJson(const QJsonObject &obj)
{
    TrackerLinkConfig cfg;
    cfg.targetType      = linkTargetFromString(obj[QStringLiteral("targetType")].toString());
    cfg.targetIndex     = obj[QStringLiteral("targetIndex")].toInt(0);
    cfg.property        = linkPropertyFromString(obj[QStringLiteral("property")].toString());
    cfg.sourceTrackIndex = obj[QStringLiteral("sourceTrackIndex")].toInt(0);
    cfg.offset          = QPointF(obj[QStringLiteral("offsetX")].toDouble(0.0),
                                  obj[QStringLiteral("offsetY")].toDouble(0.0));
    cfg.smoothing       = qBound(1, obj[QStringLiteral("smoothing")].toInt(1), 30);
    cfg.delay           = obj[QStringLiteral("delay")].toDouble(0.0);
    cfg.invertX         = obj[QStringLiteral("invertX")].toBool(false);
    cfg.invertY         = obj[QStringLiteral("invertY")].toBool(false);
    cfg.scale           = obj[QStringLiteral("scale")].toDouble(1.0);
    return cfg;
}

// ---------------------------------------------------------------------------
// TrackerLink — link management
// ---------------------------------------------------------------------------

void TrackerLink::addLink(const TrackerLinkConfig &config)
{
    TrackerLinkConfig cfg = config;
    cfg.smoothing = qBound(1, cfg.smoothing, 30);
    m_links.append(cfg);
}

void TrackerLink::removeLink(int index)
{
    if (index >= 0 && index < m_links.size())
        m_links.removeAt(index);
}

// ---------------------------------------------------------------------------
// TrackerLink — position interpolation
// ---------------------------------------------------------------------------

QPointF TrackerLink::interpolatePosition(const TrackingResult &result, double time) const
{
    if (result.regions.isEmpty() || result.fps <= 0.0)
        return {};

    double frameIdx = time * result.fps - result.startFrame;

    if (frameIdx <= 0.0) {
        const QRect &r = result.regions.first().rect;
        return QPointF(r.x() + r.width() / 2.0, r.y() + r.height() / 2.0);
    }
    if (frameIdx >= result.regions.size() - 1) {
        const QRect &r = result.regions.last().rect;
        return QPointF(r.x() + r.width() / 2.0, r.y() + r.height() / 2.0);
    }

    int lo = static_cast<int>(std::floor(frameIdx));
    int hi = lo + 1;
    double t = frameIdx - lo;

    const QRect &a = result.regions[lo].rect;
    const QRect &b = result.regions[hi].rect;

    double cx = (a.x() + a.width() / 2.0) * (1.0 - t)
              + (b.x() + b.width() / 2.0) * t;
    double cy = (a.y() + a.height() / 2.0) * (1.0 - t)
              + (b.y() + b.height() / 2.0) * t;

    return QPointF(cx, cy);
}

// ---------------------------------------------------------------------------
// TrackerLink — derive scale from region size changes
// ---------------------------------------------------------------------------

double TrackerLink::deriveScale(const TrackingResult &result, double time) const
{
    if (result.regions.size() < 2 || result.fps <= 0.0)
        return 1.0;

    double frameIdx = time * result.fps - result.startFrame;
    frameIdx = qBound(0.0, frameIdx, static_cast<double>(result.regions.size() - 1));

    int idx = qBound(0, static_cast<int>(std::round(frameIdx)), result.regions.size() - 1);

    const QRect &first = result.regions.first().rect;
    const QRect &cur   = result.regions[idx].rect;

    double firstArea = first.width() * first.height();
    if (firstArea <= 0.0)
        return 1.0;

    double curArea = cur.width() * cur.height();
    return std::sqrt(curArea / firstArea);
}

// ---------------------------------------------------------------------------
// TrackerLink — derive rotation from movement direction
// ---------------------------------------------------------------------------

double TrackerLink::deriveRotation(const TrackingResult &result, double time) const
{
    if (result.regions.size() < 2 || result.fps <= 0.0)
        return 0.0;

    double frameIdx = time * result.fps - result.startFrame;
    int idx = qBound(1, static_cast<int>(std::round(frameIdx)), result.regions.size() - 1);

    const QRect &prev = result.regions[idx - 1].rect;
    const QRect &cur  = result.regions[idx].rect;

    double dx = (cur.x() + cur.width() / 2.0) - (prev.x() + prev.width() / 2.0);
    double dy = (cur.y() + cur.height() / 2.0) - (prev.y() + prev.height() / 2.0);

    if (std::abs(dx) < 0.001 && std::abs(dy) < 0.001)
        return 0.0;

    return qRadiansToDegrees(std::atan2(dy, dx));
}

// ---------------------------------------------------------------------------
// TrackerLink — smoothing
// ---------------------------------------------------------------------------

QVector<QPointF> TrackerLink::smoothPositions(const QVector<QPointF> &positions,
                                              int windowSize)
{
    if (positions.isEmpty() || windowSize <= 1)
        return positions;

    windowSize = qBound(1, windowSize, 30);
    int half = windowSize / 2;

    QVector<QPointF> result;
    result.reserve(positions.size());

    for (int i = 0; i < positions.size(); ++i) {
        int lo = qMax(0, i - half);
        int hi = qMin(positions.size() - 1, i + half);
        int count = hi - lo + 1;

        double sx = 0.0, sy = 0.0;
        for (int j = lo; j <= hi; ++j) {
            sx += positions[j].x();
            sy += positions[j].y();
        }
        result.append(QPointF(sx / count, sy / count));
    }

    return result;
}

// ---------------------------------------------------------------------------
// TrackerLink — public queries
// ---------------------------------------------------------------------------

QPointF TrackerLink::getLinkedPosition(const TrackingResult &result, double time) const
{
    if (result.isEmpty())
        return {};

    // Collect raw positions for smoothing window around the target time
    int smoothWindow = m_links.isEmpty() ? 1 : m_links.first().smoothing;
    double delayVal  = m_links.isEmpty() ? 0.0 : m_links.first().delay;
    QPointF offsetVal = m_links.isEmpty() ? QPointF() : m_links.first().offset;
    bool invX = !m_links.isEmpty() && m_links.first().invertX;
    bool invY = !m_links.isEmpty() && m_links.first().invertY;
    double scaleMul = m_links.isEmpty() ? 1.0 : m_links.first().scale;

    double adjustedTime = time - delayVal;

    if (smoothWindow <= 1) {
        QPointF pos = interpolatePosition(result, adjustedTime);
        double px = pos.x() * scaleMul + offsetVal.x();
        double py = pos.y() * scaleMul + offsetVal.y();
        if (invX) px = -px + 2.0 * offsetVal.x();
        if (invY) py = -py + 2.0 * offsetVal.y();
        return QPointF(px, py);
    }

    // Gather positions across the smoothing window
    int half = smoothWindow / 2;
    double frameDur = (result.fps > 0.0) ? (1.0 / result.fps) : (1.0 / 30.0);

    QVector<QPointF> samples;
    samples.reserve(smoothWindow);
    for (int i = -half; i <= half; ++i)
        samples.append(interpolatePosition(result, adjustedTime + i * frameDur));

    QVector<QPointF> smoothed = smoothPositions(samples, smoothWindow);
    QPointF pos = smoothed[half]; // centre of the window

    double px = pos.x() * scaleMul + offsetVal.x();
    double py = pos.y() * scaleMul + offsetVal.y();
    if (invX) px = -px + 2.0 * offsetVal.x();
    if (invY) py = -py + 2.0 * offsetVal.y();

    return QPointF(px, py);
}

double TrackerLink::getLinkedValue(const TrackingResult &result, double time,
                                   LinkProperty property) const
{
    if (result.isEmpty())
        return 0.0;

    double delayVal = m_links.isEmpty() ? 0.0 : m_links.first().delay;
    double scaleMul = m_links.isEmpty() ? 1.0 : m_links.first().scale;
    double adjustedTime = time - delayVal;

    switch (property) {
    case LinkProperty::PositionX: {
        QPointF pos = getLinkedPosition(result, time);
        return pos.x();
    }
    case LinkProperty::PositionY: {
        QPointF pos = getLinkedPosition(result, time);
        return pos.y();
    }
    case LinkProperty::ScaleX:
    case LinkProperty::ScaleY:
        return deriveScale(result, adjustedTime) * scaleMul;
    case LinkProperty::Rotation:
        return deriveRotation(result, adjustedTime);
    case LinkProperty::Opacity:
        // Derive opacity from tracking confidence at the given time
        if (result.fps > 0.0) {
            double frameIdx = adjustedTime * result.fps - result.startFrame;
            int idx = qBound(0, static_cast<int>(std::round(frameIdx)),
                             result.regions.size() - 1);
            return result.regions[idx].confidence;
        }
        return 1.0;
    }
    return 0.0;
}

QMap<int, QPointF> TrackerLink::applyLinks(const TrackingResult &result,
                                           double time) const
{
    QMap<int, QPointF> output;

    for (const TrackerLinkConfig &cfg : m_links) {
        double adjustedTime = time - cfg.delay;
        int smoothWindow = qBound(1, cfg.smoothing, 30);

        QPointF pos;
        if (smoothWindow <= 1) {
            pos = interpolatePosition(result, adjustedTime);
        } else {
            int half = smoothWindow / 2;
            double frameDur = (result.fps > 0.0) ? (1.0 / result.fps) : (1.0 / 30.0);

            QVector<QPointF> samples;
            samples.reserve(smoothWindow);
            for (int i = -half; i <= half; ++i)
                samples.append(interpolatePosition(result, adjustedTime + i * frameDur));

            QVector<QPointF> smoothed = smoothPositions(samples, smoothWindow);
            pos = smoothed[half];
        }

        double px = pos.x() * cfg.scale + cfg.offset.x();
        double py = pos.y() * cfg.scale + cfg.offset.y();
        if (cfg.invertX) px = -px + 2.0 * cfg.offset.x();
        if (cfg.invertY) py = -py + 2.0 * cfg.offset.y();

        output[cfg.targetIndex] = QPointF(px, py);
    }

    return output;
}

// ---------------------------------------------------------------------------
// TrackerLink — serialisation
// ---------------------------------------------------------------------------

QJsonObject TrackerLink::toJson() const
{
    QJsonArray arr;
    for (const TrackerLinkConfig &cfg : m_links)
        arr.append(cfg.toJson());

    QJsonObject obj;
    obj[QStringLiteral("links")] = arr;
    return obj;
}

void TrackerLink::fromJson(const QJsonObject &obj)
{
    m_links.clear();

    const QJsonArray arr = obj[QStringLiteral("links")].toArray();
    for (const QJsonValue &val : arr)
        m_links.append(TrackerLinkConfig::fromJson(val.toObject()));
}
