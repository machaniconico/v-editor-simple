#pragma once

#include <array>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QPolygonF>
#include <QString>
#include <QVector>

#include "MotionTracker.h"
#include "PlanarTracker.h"
#include "TextManager.h"

// --- Link target type ---

enum class LinkTarget {
    TextOverlay,
    ImageOverlay,
    Effect,
    Mask,
    ShapeLayer,
    ParticleEmitter
};

// --- TrackerLink mode (AC1: Point=0 for backward compat) ---

enum class Mode { Point = 0, Planar };

// --- Linked property ---

enum class LinkProperty {
    PositionX,
    PositionY,
    ScaleX,
    ScaleY,
    Rotation,
    Opacity
};

// --- Per-link configuration ---

struct TrackerLinkConfig {
    LinkTarget targetType = LinkTarget::TextOverlay;
    int targetIndex = 0;

    LinkProperty property = LinkProperty::PositionX;
    int sourceTrackIndex = 0;

    QPointF offset;                 // constant offset from tracked position
    int smoothing = 1;              // frames to smooth (1-30)
    double delay = 0.0;            // seconds

    bool invertX = false;
    bool invertY = false;
    double scale = 1.0;            // multiplier

    QJsonObject toJson() const;
    static TrackerLinkConfig fromJson(const QJsonObject &obj);
};

// --- Tracker link manager ---

class TrackerLink
{
public:
    TrackerLink() = default;

    // --- Link management ---

    void addLink(const TrackerLinkConfig &config);
    void removeLink(int index);
    const QVector<TrackerLinkConfig> &links() const { return m_links; }

    // --- Position / value queries ---

    // Apply motion tracking result as per-frame position keyframes on an overlay
    static void applyToOverlay(EnhancedTextOverlay *overlay,
                               const TrackingResult &result,
                               double frameRate);

    // Get smoothed tracked position at a given time
    QPointF getLinkedPosition(const TrackingResult &result, double time) const;

    // Get tracked value for any linked property
    double getLinkedValue(const TrackingResult &result, double time,
                          LinkProperty property) const;

    // Return mapping of target indices to positions for all links
    QMap<int, QPointF> applyLinks(const TrackingResult &result, double time) const;

    // --- Smoothing utility ---

    static QVector<QPointF> smoothPositions(const QVector<QPointF> &positions,
                                            int windowSize);

    // --- Planar mode (AC1) ---

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setPlanarTrack(const planartrack::PlanarTrack *track);
    void setRefQuad(const planartrack::Quad &quad) { m_refQuad = quad; }
    planartrack::Quad refQuad() const { return m_refQuad; }

    QPolygonF computePlanarQuad(int frameIndex) const;

    // --- Serialisation ---

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    // Interpolate position between adjacent tracking regions
    QPointF interpolatePosition(const TrackingResult &result, double time) const;

    // Derive scale factor from region size changes over time
    double deriveScale(const TrackingResult &result, double time) const;

    // Derive rotation from region centre movement direction
    double deriveRotation(const TrackingResult &result, double time) const;

    // --- Per-corner 2D Kalman filter (AC3) ---

    struct KalmanCorner {
        double x = 0.0, y = 0.0;
        double vx = 0.0, vy = 0.0;
        bool initialized = false;
    };

    QPointF kalmanPredict(KalmanCorner &state, double dt) const;
    QPointF kalmanUpdate(KalmanCorner &state, const QPointF &measurement,
                         double smoothingStrength) const;
    void resetKalmanState() const;

    Mode m_mode = Mode::Point;
    const planartrack::PlanarTrack *m_planarTrack = nullptr;
    planartrack::Quad m_refQuad;
    mutable std::array<KalmanCorner, 4> m_kalmanState;
    mutable int m_lastPlanarFrame = -1;

    QVector<TrackerLinkConfig> m_links;
};
