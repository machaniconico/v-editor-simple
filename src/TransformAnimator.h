#pragma once

#include "Keyframe.h"

#include <QImage>
#include <QPointF>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>

// --- Transform property enum ---

enum class TransformProperty {
    PositionX,
    PositionY,
    ScaleX,
    ScaleY,
    Rotation,
    Opacity,
    AnchorX,
    AnchorY,
    SkewX,
    SkewY
};

// --- Slide direction for presets ---

enum class SlideDirection { Left, Right, Top, Bottom };

// --- Transform state at a point in time ---

struct TransformState {
    double posX = 0.0;
    double posY = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0;   // degrees
    double opacity = 1.0;    // 0.0 to 1.0
    double anchorX = 0.0;
    double anchorY = 0.0;
    double skewX = 0.0;      // degrees
    double skewY = 0.0;      // degrees

    bool isDefault() const {
        return posX == 0.0 && posY == 0.0
            && scaleX == 1.0 && scaleY == 1.0
            && rotation == 0.0 && opacity == 1.0
            && anchorX == 0.0 && anchorY == 0.0
            && skewX == 0.0 && skewY == 0.0;
    }

    void reset() { *this = TransformState{}; }
};

// --- Transform Animator ---

class TransformAnimator
{
public:
    TransformAnimator();

    // --- Keyframe management ---

    void setKeyframe(double time, TransformProperty property, double value,
                     KeyframePoint::Interpolation interp = KeyframePoint::Linear);
    void removeKeyframe(double time, TransformProperty property);

    // --- Evaluation ---

    TransformState getTransformAt(double time) const;

    // --- Image processing ---

    static QImage applyTransform(const QImage &image, const TransformState &state);

    // --- Motion path for visualization ---

    QVector<QPointF> generateMotionPath(TransformProperty property,
                                        double startTime, double endTime,
                                        int samples = 100) const;

    // --- Query ---

    bool hasAnimation() const;
    QVector<double> allKeyframeTimes() const;

    // --- Per-property access ---

    KeyframeTrack *track(TransformProperty property);
    const KeyframeTrack *track(TransformProperty property) const;

    // --- Serialisation ---

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    // --- Built-in animation presets ---

    static TransformAnimator createSlideIn(SlideDirection direction, double duration,
                                           double distance = 1920.0);
    static TransformAnimator createFadeIn(double duration);
    static TransformAnimator createFadeOut(double duration);
    static TransformAnimator createZoomIn(double duration);
    static TransformAnimator createZoomOut(double duration);
    static TransformAnimator createSpin(double duration, int rotations = 1);
    static TransformAnimator createBounce(double duration);

    // --- Utility ---

    static QString propertyName(TransformProperty property);
    static TransformProperty propertyFromName(const QString &name);
    static double propertyDefaultValue(TransformProperty property);

private:
    // One KeyframeTrack per TransformProperty (indexed by enum value)
    QVector<KeyframeTrack> m_tracks;

    void ensureTracks();
    int trackIndex(TransformProperty property) const;
};
