#include "Camera3D.h"

#include <QPainter>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <set>

// ============================================================
// Camera3DState — serialisation
// ============================================================

QJsonObject Camera3DState::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("posX")]      = static_cast<double>(position.x());
    obj[QStringLiteral("posY")]      = static_cast<double>(position.y());
    obj[QStringLiteral("posZ")]      = static_cast<double>(position.z());
    obj[QStringLiteral("targetX")]   = static_cast<double>(target.x());
    obj[QStringLiteral("targetY")]   = static_cast<double>(target.y());
    obj[QStringLiteral("targetZ")]   = static_cast<double>(target.z());
    obj[QStringLiteral("fov")]       = fov;
    obj[QStringLiteral("nearPlane")] = nearPlane;
    obj[QStringLiteral("farPlane")]  = farPlane;
    obj[QStringLiteral("roll")]      = roll;
    return obj;
}

Camera3DState Camera3DState::fromJson(const QJsonObject &obj)
{
    Camera3DState s;
    s.position = QVector3D(
        static_cast<float>(obj[QStringLiteral("posX")].toDouble(0.0)),
        static_cast<float>(obj[QStringLiteral("posY")].toDouble(0.0)),
        static_cast<float>(obj[QStringLiteral("posZ")].toDouble(0.0)));
    s.target = QVector3D(
        static_cast<float>(obj[QStringLiteral("targetX")].toDouble(0.0)),
        static_cast<float>(obj[QStringLiteral("targetY")].toDouble(0.0)),
        static_cast<float>(obj[QStringLiteral("targetZ")].toDouble(-1.0)));
    s.fov       = obj[QStringLiteral("fov")].toDouble(60.0);
    s.nearPlane = obj[QStringLiteral("nearPlane")].toDouble(0.1);
    s.farPlane  = obj[QStringLiteral("farPlane")].toDouble(1000.0);
    s.roll      = obj[QStringLiteral("roll")].toDouble(0.0);
    return s;
}

// ============================================================
// Layer3DTransform — serialisation
// ============================================================

QJsonObject Layer3DTransform::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("positionZ")] = positionZ;
    obj[QStringLiteral("rotationX")] = rotationX;
    obj[QStringLiteral("rotationY")] = rotationY;
    obj[QStringLiteral("rotationZ")] = rotationZ;
    return obj;
}

Layer3DTransform Layer3DTransform::fromJson(const QJsonObject &obj)
{
    Layer3DTransform t;
    t.positionZ = obj[QStringLiteral("positionZ")].toDouble(0.0);
    t.rotationX = obj[QStringLiteral("rotationX")].toDouble(0.0);
    t.rotationY = obj[QStringLiteral("rotationY")].toDouble(0.0);
    t.rotationZ = obj[QStringLiteral("rotationZ")].toDouble(0.0);
    return t;
}

// ============================================================
// Camera3D — property helpers
// ============================================================

QString Camera3D::propertyName(Camera3DProperty property)
{
    switch (property) {
    case Camera3DProperty::PositionX: return QStringLiteral("cam.positionX");
    case Camera3DProperty::PositionY: return QStringLiteral("cam.positionY");
    case Camera3DProperty::PositionZ: return QStringLiteral("cam.positionZ");
    case Camera3DProperty::TargetX:   return QStringLiteral("cam.targetX");
    case Camera3DProperty::TargetY:   return QStringLiteral("cam.targetY");
    case Camera3DProperty::TargetZ:   return QStringLiteral("cam.targetZ");
    case Camera3DProperty::Fov:       return QStringLiteral("cam.fov");
    case Camera3DProperty::Roll:      return QStringLiteral("cam.roll");
    case Camera3DProperty::Count:     break;
    }
    return QStringLiteral("unknown");
}

Camera3DProperty Camera3D::propertyFromName(const QString &name)
{
    if (name == QLatin1String("cam.positionX")) return Camera3DProperty::PositionX;
    if (name == QLatin1String("cam.positionY")) return Camera3DProperty::PositionY;
    if (name == QLatin1String("cam.positionZ")) return Camera3DProperty::PositionZ;
    if (name == QLatin1String("cam.targetX"))   return Camera3DProperty::TargetX;
    if (name == QLatin1String("cam.targetY"))   return Camera3DProperty::TargetY;
    if (name == QLatin1String("cam.targetZ"))   return Camera3DProperty::TargetZ;
    if (name == QLatin1String("cam.fov"))       return Camera3DProperty::Fov;
    if (name == QLatin1String("cam.roll"))      return Camera3DProperty::Roll;
    return Camera3DProperty::PositionX; // fallback
}

double Camera3D::propertyDefaultValue(Camera3DProperty property)
{
    switch (property) {
    case Camera3DProperty::TargetZ:  return -1.0;
    case Camera3DProperty::Fov:      return 60.0;
    default:                         return 0.0;
    }
}

// ============================================================
// Camera3D — constructor / track management
// ============================================================

Camera3D::Camera3D()
{
    ensureTracks();
}

void Camera3D::ensureTracks()
{
    const int count = static_cast<int>(Camera3DProperty::Count);
    if (m_tracks.size() == count)
        return;

    m_tracks.clear();
    m_tracks.reserve(count);
    for (int i = 0; i < count; ++i) {
        auto prop = static_cast<Camera3DProperty>(i);
        m_tracks.append(KeyframeTrack(propertyName(prop), propertyDefaultValue(prop)));
    }
}

int Camera3D::trackIndex(Camera3DProperty property) const
{
    return static_cast<int>(property);
}

void Camera3D::ensureLayerIndex(int index)
{
    while (m_layerTransforms.size() <= index)
        m_layerTransforms.append(Layer3DTransform{});
}

// ============================================================
// Camera3D — camera state
// ============================================================

void Camera3D::setCamera(const Camera3DState &state)
{
    m_state = state;
}

// ============================================================
// Camera3D — layer depth / 3D transform
// ============================================================

void Camera3D::setLayerDepth(int layerIndex, double z)
{
    ensureLayerIndex(layerIndex);
    m_layerTransforms[layerIndex].positionZ = z;
}

double Camera3D::layerDepth(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= m_layerTransforms.size())
        return 0.0;
    return m_layerTransforms[layerIndex].positionZ;
}

void Camera3D::setLayer3DTransform(int layerIndex, const Layer3DTransform &transform)
{
    ensureLayerIndex(layerIndex);
    m_layerTransforms[layerIndex] = transform;
}

Layer3DTransform Camera3D::layer3DTransform(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= m_layerTransforms.size())
        return Layer3DTransform{};
    return m_layerTransforms[layerIndex];
}

// ============================================================
// Camera3D — 3D-to-2D projection
// ============================================================

QPointF Camera3D::projectTo2D(const QVector3D &point3D, const QSize &canvasSize) const
{
    // Translate point relative to camera position
    double x = static_cast<double>(point3D.x()) - static_cast<double>(m_state.position.x());
    double y = static_cast<double>(point3D.y()) - static_cast<double>(m_state.position.y());
    double z = static_cast<double>(point3D.z()) - static_cast<double>(m_state.position.z());

    // Perspective projection: x' = x * fov / (fov + z)
    double focalLength = m_state.fov;
    double denom = focalLength + z;
    if (std::abs(denom) < 0.001)
        denom = 0.001; // prevent division by zero

    double projX = x * focalLength / denom;
    double projY = y * focalLength / denom;

    // Map to canvas coordinates (center-origin to top-left-origin)
    double screenX = canvasSize.width() / 2.0 + projX;
    double screenY = canvasSize.height() / 2.0 + projY;

    return QPointF(screenX, screenY);
}

// ============================================================
// Camera3D — perspective transform for a single layer image
// ============================================================

QImage Camera3D::applyPerspective(const QImage &image,
                                  const Layer3DTransform &layer3D,
                                  const Camera3DState &cameraState,
                                  const QSize &canvasSize)
{
    if (image.isNull())
        return image;

    // Short-circuit for identity 3D transform
    if (layer3D.isDefault())
        return image;

    double focalLength = cameraState.fov;
    double z = layer3D.positionZ - static_cast<double>(cameraState.position.z());

    // Perspective scale factor
    double denom = focalLength + z;
    if (std::abs(denom) < 0.001)
        denom = 0.001;
    double scaleFactor = focalLength / denom;

    QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    double cx = canvasSize.width() / 2.0;
    double cy = canvasSize.height() / 2.0;

    QTransform transform;
    // Move origin to canvas center
    transform.translate(cx, cy);

    // Apply perspective scale (simulates Z-depth)
    transform.scale(scaleFactor, scaleFactor);

    // Apply 3D rotations via perspective shear approximations
    if (std::abs(layer3D.rotationY) > 0.001) {
        double radY = layer3D.rotationY * M_PI / 180.0;
        double shearX = std::sin(radY) * 0.5;
        double scaleXfactor = std::cos(radY);
        transform.shear(shearX, 0.0);
        transform.scale(scaleXfactor, 1.0);
    }

    if (std::abs(layer3D.rotationX) > 0.001) {
        double radX = layer3D.rotationX * M_PI / 180.0;
        double shearY = std::sin(radX) * 0.5;
        double scaleYfactor = std::cos(radX);
        transform.shear(0.0, shearY);
        transform.scale(1.0, scaleYfactor);
    }

    // Z-rotation (standard 2D rotation)
    if (std::abs(layer3D.rotationZ) > 0.001)
        transform.rotate(layer3D.rotationZ);

    // Move origin back, centering the image
    transform.translate(-image.width() / 2.0, -image.height() / 2.0);

    painter.setTransform(transform);
    painter.drawImage(0, 0, image);
    painter.end();

    return result;
}

// ============================================================
// Camera3D — scene rendering
// ============================================================

QImage Camera3D::renderScene(const QVector<CompositeLayer> &layers,
                             const QVector<QImage> &layerImages,
                             const QSize &canvasSize, double time)
{
    // Update camera state from keyframes if animated
    if (hasAnimation())
        m_state = getCameraAt(time);

    // Build index list sorted by Z-depth (painter's algorithm: far to near)
    struct DepthEntry {
        int index;
        double z;
    };

    QVector<DepthEntry> depthOrder;
    depthOrder.reserve(layers.size());
    for (int i = 0; i < layers.size(); ++i) {
        double z = (i < m_layerTransforms.size()) ? m_layerTransforms[i].positionZ : 0.0;
        depthOrder.append({i, z});
    }

    // Sort far-to-near (larger Z = farther away, painted first)
    std::sort(depthOrder.begin(), depthOrder.end(),
              [](const DepthEntry &a, const DepthEntry &b) {
                  return a.z > b.z;
              });

    // Composite
    QImage result(canvasSize, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    for (const auto &entry : depthOrder) {
        int i = entry.index;
        if (i >= layerImages.size() || layerImages[i].isNull())
            continue;

        if (!layers[i].visible)
            continue;

        // Get 3D transform for this layer
        Layer3DTransform lt = (i < m_layerTransforms.size())
                                  ? m_layerTransforms[i]
                                  : Layer3DTransform{};

        // Apply perspective transform
        QImage transformed = applyPerspective(layerImages[i], lt, m_state, canvasSize);

        // Draw with layer opacity and blend
        painter.setOpacity(std::clamp(layers[i].opacity, 0.0, 1.0));
        painter.drawImage(0, 0, transformed);
    }

    painter.end();
    return result;
}

// ============================================================
// Camera3D — keyframe management
// ============================================================

void Camera3D::setCameraKeyframe(double time, const Camera3DState &state,
                                 KeyframePoint::Interpolation interp)
{
    m_tracks[trackIndex(Camera3DProperty::PositionX)].addKeyframe(
        time, static_cast<double>(state.position.x()), interp);
    m_tracks[trackIndex(Camera3DProperty::PositionY)].addKeyframe(
        time, static_cast<double>(state.position.y()), interp);
    m_tracks[trackIndex(Camera3DProperty::PositionZ)].addKeyframe(
        time, static_cast<double>(state.position.z()), interp);
    m_tracks[trackIndex(Camera3DProperty::TargetX)].addKeyframe(
        time, static_cast<double>(state.target.x()), interp);
    m_tracks[trackIndex(Camera3DProperty::TargetY)].addKeyframe(
        time, static_cast<double>(state.target.y()), interp);
    m_tracks[trackIndex(Camera3DProperty::TargetZ)].addKeyframe(
        time, static_cast<double>(state.target.z()), interp);
    m_tracks[trackIndex(Camera3DProperty::Fov)].addKeyframe(
        time, state.fov, interp);
    m_tracks[trackIndex(Camera3DProperty::Roll)].addKeyframe(
        time, state.roll, interp);
}

Camera3DState Camera3D::getCameraAt(double time) const
{
    Camera3DState state;
    state.position = QVector3D(
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::PositionX)].valueAt(time)),
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::PositionY)].valueAt(time)),
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::PositionZ)].valueAt(time)));
    state.target = QVector3D(
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::TargetX)].valueAt(time)),
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::TargetY)].valueAt(time)),
        static_cast<float>(m_tracks[trackIndex(Camera3DProperty::TargetZ)].valueAt(time)));
    state.fov  = m_tracks[trackIndex(Camera3DProperty::Fov)].valueAt(time);
    state.roll = m_tracks[trackIndex(Camera3DProperty::Roll)].valueAt(time);

    // Preserve near/far from current state (not typically animated)
    state.nearPlane = m_state.nearPlane;
    state.farPlane  = m_state.farPlane;

    return state;
}

// ============================================================
// Camera3D — animation query
// ============================================================

bool Camera3D::hasAnimation() const
{
    for (const auto &track : m_tracks)
        if (track.hasKeyframes()) return true;
    return false;
}

QVector<double> Camera3D::allKeyframeTimes() const
{
    std::set<double> times;
    for (const auto &track : m_tracks) {
        for (const auto &kf : track.keyframes()) {
            double rounded = std::round(kf.time * 1000.0) / 1000.0;
            times.insert(rounded);
        }
    }
    QVector<double> result;
    result.reserve(static_cast<int>(times.size()));
    for (double t : times)
        result.append(t);
    return result;
}

// ============================================================
// Camera3D — per-property keyframe access
// ============================================================

KeyframeTrack *Camera3D::track(Camera3DProperty property)
{
    return &m_tracks[trackIndex(property)];
}

const KeyframeTrack *Camera3D::track(Camera3DProperty property) const
{
    return &m_tracks[trackIndex(property)];
}

// ============================================================
// Camera3D — built-in camera moves
// ============================================================

Camera3D Camera3D::createDollyZoom(double startZ, double endZ, double duration)
{
    Camera3D cam;

    Camera3DState startState;
    startState.position = QVector3D(0.0f, 0.0f, static_cast<float>(startZ));

    Camera3DState endState;
    endState.position = QVector3D(0.0f, 0.0f, static_cast<float>(endZ));

    // Adjust FOV inversely to maintain subject size (Hitchcock zoom)
    double startFov = 60.0;
    double fovRatio = (startFov + startZ) / (startFov + endZ);
    double endFov = startFov * fovRatio;

    startState.fov = startFov;
    endState.fov   = endFov;

    cam.setCameraKeyframe(0.0, startState, KeyframePoint::EaseInOut);
    cam.setCameraKeyframe(duration, endState, KeyframePoint::EaseInOut);
    return cam;
}

Camera3D Camera3D::createPanShot(double startX, double endX, double duration)
{
    Camera3D cam;

    Camera3DState startState;
    startState.position = QVector3D(static_cast<float>(startX), 0.0f, 0.0f);
    startState.target   = QVector3D(static_cast<float>(startX), 0.0f, -1.0f);

    Camera3DState endState;
    endState.position = QVector3D(static_cast<float>(endX), 0.0f, 0.0f);
    endState.target   = QVector3D(static_cast<float>(endX), 0.0f, -1.0f);

    cam.setCameraKeyframe(0.0, startState, KeyframePoint::EaseInOut);
    cam.setCameraKeyframe(duration, endState, KeyframePoint::EaseInOut);
    return cam;
}

Camera3D Camera3D::createOrbitShot(const QVector3D &centerPoint, double radius,
                                   double duration)
{
    Camera3D cam;

    // Create keyframes around a circle (8 sample points for smooth orbit)
    const int steps = 8;
    for (int i = 0; i <= steps; ++i) {
        double t = (duration * i) / steps;
        double angle = (2.0 * M_PI * i) / steps;

        Camera3DState state;
        state.position = QVector3D(
            centerPoint.x() + static_cast<float>(radius * std::cos(angle)),
            centerPoint.y(),
            centerPoint.z() + static_cast<float>(radius * std::sin(angle)));
        state.target = centerPoint;

        auto interp = (i == 0 || i == steps) ? KeyframePoint::EaseInOut
                                              : KeyframePoint::Linear;
        cam.setCameraKeyframe(t, state, interp);
    }
    return cam;
}

Camera3D Camera3D::createZoomShot(double startFov, double endFov, double duration)
{
    Camera3D cam;

    Camera3DState startState;
    startState.fov = startFov;

    Camera3DState endState;
    endState.fov = endFov;

    cam.setCameraKeyframe(0.0, startState, KeyframePoint::EaseInOut);
    cam.setCameraKeyframe(duration, endState, KeyframePoint::EaseInOut);
    return cam;
}

// ============================================================
// Camera3D — serialisation
// ============================================================

QJsonObject Camera3D::toJson() const
{
    QJsonObject obj;

    // Current camera state
    obj[QStringLiteral("cameraState")] = m_state.toJson();

    // Keyframe tracks
    QJsonArray tracksArr;
    for (const auto &track : m_tracks) {
        if (!track.hasKeyframes())
            continue;

        QJsonObject trackObj;
        trackObj[QStringLiteral("property")] = track.propertyName();

        QJsonArray kfArr;
        for (const auto &kf : track.keyframes()) {
            QJsonObject kfObj;
            kfObj[QStringLiteral("time")]          = kf.time;
            kfObj[QStringLiteral("value")]         = kf.value;
            kfObj[QStringLiteral("interpolation")] = static_cast<int>(kf.interpolation);
            kfArr.append(kfObj);
        }
        trackObj[QStringLiteral("keyframes")] = kfArr;
        tracksArr.append(trackObj);
    }
    obj[QStringLiteral("tracks")] = tracksArr;

    // Layer 3D transforms
    QJsonArray layerArr;
    for (const auto &lt : m_layerTransforms)
        layerArr.append(lt.toJson());
    obj[QStringLiteral("layerTransforms")] = layerArr;

    return obj;
}

void Camera3D::fromJson(const QJsonObject &obj)
{
    // Camera state
    m_state = Camera3DState::fromJson(
        obj[QStringLiteral("cameraState")].toObject());

    // Keyframe tracks
    ensureTracks();
    QJsonArray tracksArr = obj[QStringLiteral("tracks")].toArray();
    for (const auto &trackVal : tracksArr) {
        QJsonObject trackObj = trackVal.toObject();
        QString propName = trackObj[QStringLiteral("property")].toString();
        Camera3DProperty prop = propertyFromName(propName);
        int idx = trackIndex(prop);

        m_tracks[idx] = KeyframeTrack(propName, propertyDefaultValue(prop));

        QJsonArray kfArr = trackObj[QStringLiteral("keyframes")].toArray();
        for (const auto &kfVal : kfArr) {
            QJsonObject kfObj = kfVal.toObject();
            double time  = kfObj[QStringLiteral("time")].toDouble();
            double value = kfObj[QStringLiteral("value")].toDouble();
            auto interp  = static_cast<KeyframePoint::Interpolation>(
                kfObj[QStringLiteral("interpolation")].toInt(0));
            m_tracks[idx].addKeyframe(time, value, interp);
        }
    }

    // Layer 3D transforms
    m_layerTransforms.clear();
    QJsonArray layerArr = obj[QStringLiteral("layerTransforms")].toArray();
    for (const auto &ltVal : layerArr)
        m_layerTransforms.append(Layer3DTransform::fromJson(ltVal.toObject()));
}
