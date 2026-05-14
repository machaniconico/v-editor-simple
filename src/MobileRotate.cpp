#include "MobileRotate.h"
#include <QTransform>

namespace mobile::rotate {

// ---------------------------------------------------------------------------
// isPortraitTarget
// ---------------------------------------------------------------------------
bool isPortraitTarget(const DeviceProfile& device)
{
    return device.maxResolution.height() > device.maxResolution.width();
}

// ---------------------------------------------------------------------------
// computeRotation
// ---------------------------------------------------------------------------
RotateDecision computeRotation(const QSize& source, const DeviceProfile& device)
{
    RotateDecision decision;
    decision.angleDeg           = 0;
    decision.needsRotate        = false;
    decision.suggestedReframeMode = reframe::Mode::CenterCrop;

    const bool sourceLandscape = source.width() > source.height();
    const bool targetPortrait  = isPortraitTarget(device);

    if (sourceLandscape && targetPortrait) {
        // Landscape source going to portrait target → rotate 90° CW
        decision.needsRotate          = true;
        decision.angleDeg             = 90;
        decision.suggestedReframeMode = reframe::Mode::SmartCenterFollow;
    }
    // All other combinations (portrait→portrait, landscape→landscape,
    // portrait→landscape) need no rotation.

    return decision;
}

// ---------------------------------------------------------------------------
// applyRotation
// ---------------------------------------------------------------------------
QImage applyRotation(const QImage& image, int angleDeg)
{
    if (image.isNull())
        return image;

    // Normalise to 0 / 90 / 180 / 270
    const int angle = ((angleDeg % 360) + 360) % 360;

    if (angle == 0)
        return image;

    QTransform transform;
    transform.rotate(angle);
    return image.transformed(transform, Qt::SmoothTransformation);
}

} // namespace mobile::rotate
