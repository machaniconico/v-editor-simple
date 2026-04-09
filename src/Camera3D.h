#pragma once

#include "Keyframe.h"
#include "LayerCompositor.h"

#include <QImage>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
#include <QSize>
#include <QVector>
#include <QVector3D>

// --- Camera state at a point in time ---

struct Camera3DState {
    QVector3D position  = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D target    = QVector3D(0.0f, 0.0f, -1.0f);
    double fov          = 60.0;    // degrees
    double nearPlane    = 0.1;
    double farPlane     = 1000.0;
    double roll         = 0.0;     // degrees

    bool isDefault() const {
        return position == QVector3D(0.0f, 0.0f, 0.0f)
            && target == QVector3D(0.0f, 0.0f, -1.0f)
            && fov == 60.0 && nearPlane == 0.1
            && farPlane == 1000.0 && roll == 0.0;
    }

    void reset() { *this = Camera3DState{}; }

    QJsonObject toJson() const;
    static Camera3DState fromJson(const QJsonObject &obj);
};

// --- Per-layer 3D transform extension ---

struct Layer3DTransform {
    double positionZ = 0.0;     // depth in 3D space
    double rotationX = 0.0;     // degrees
    double rotationY = 0.0;     // degrees
    double rotationZ = 0.0;     // degrees

    bool isDefault() const {
        return positionZ == 0.0 && rotationX == 0.0
            && rotationY == 0.0 && rotationZ == 0.0;
    }

    void reset() { *this = Layer3DTransform{}; }

    QJsonObject toJson() const;
    static Layer3DTransform fromJson(const QJsonObject &obj);
};

// --- Camera property enum for keyframe tracks ---

enum class Camera3DProperty {
    PositionX,
    PositionY,
    PositionZ,
    TargetX,
    TargetY,
    TargetZ,
    Fov,
    Roll,
    Count   // sentinel — must remain last
};

// --- Camera 3D ---

class Camera3D
{
public:
    Camera3D();

    // --- Camera state ---

    void setCamera(const Camera3DState &state);
    Camera3DState camera() const { return m_state; }

    // --- Layer depth ---

    void setLayerDepth(int layerIndex, double z);
    double layerDepth(int layerIndex) const;

    void setLayer3DTransform(int layerIndex, const Layer3DTransform &transform);
    Layer3DTransform layer3DTransform(int layerIndex) const;

    // --- 3D projection ---

    QPointF projectTo2D(const QVector3D &point3D, const QSize &canvasSize) const;

    // --- Scene rendering ---

    QImage renderScene(const QVector<CompositeLayer> &layers,
                       const QVector<QImage> &layerImages,
                       const QSize &canvasSize, double time);

    // --- Perspective transform (static utility) ---

    static QImage applyPerspective(const QImage &image,
                                   const Layer3DTransform &layer3D,
                                   const Camera3DState &cameraState,
                                   const QSize &canvasSize);

    // --- Camera keyframes ---

    void setCameraKeyframe(double time, const Camera3DState &state,
                           KeyframePoint::Interpolation interp = KeyframePoint::Linear);
    Camera3DState getCameraAt(double time) const;

    bool hasAnimation() const;
    QVector<double> allKeyframeTimes() const;

    // --- Per-property keyframe access ---

    KeyframeTrack *track(Camera3DProperty property);
    const KeyframeTrack *track(Camera3DProperty property) const;

    // --- Built-in camera moves (static factory methods) ---

    static Camera3D createDollyZoom(double startZ, double endZ, double duration);
    static Camera3D createPanShot(double startX, double endX, double duration);
    static Camera3D createOrbitShot(const QVector3D &centerPoint, double radius,
                                    double duration);
    static Camera3D createZoomShot(double startFov, double endFov, double duration);

    // --- Serialisation ---

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

    // --- Utility ---

    static QString propertyName(Camera3DProperty property);
    static Camera3DProperty propertyFromName(const QString &name);
    static double propertyDefaultValue(Camera3DProperty property);

private:
    Camera3DState m_state;

    // One KeyframeTrack per Camera3DProperty (indexed by enum value)
    QVector<KeyframeTrack> m_tracks;

    // Per-layer 3D transforms (indexed by layer index)
    QVector<Layer3DTransform> m_layerTransforms;

    void ensureTracks();
    int trackIndex(Camera3DProperty property) const;
    void ensureLayerIndex(int index);
};
