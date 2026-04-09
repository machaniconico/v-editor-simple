#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVector>

#include "MotionTracker.h"

// --- Link target type ---

enum class LinkTarget {
    TextOverlay,
    ImageOverlay,
    Effect,
    Mask,
    ShapeLayer,
    ParticleEmitter
};

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

    QVector<TrackerLinkConfig> m_links;
};
