#pragma once
#include <QImage>
#include <QSize>
#include "MobileDeviceProfile.h"
#include "AspectReframer.h"

namespace mobile::rotate {

struct RotateDecision {
    bool            needsRotate;
    int             angleDeg;          // 0, 90, 180, or 270
    reframe::Mode   suggestedReframeMode;
};

// Returns true when the device target is portrait (resolution.height > resolution.width).
bool isPortraitTarget(const DeviceProfile& device);

// Compute whether a rotation is needed and by how many degrees.
// source : original video dimensions (width x height)
// device : target device profile
RotateDecision computeRotation(const QSize& source, const DeviceProfile& device);

// Apply a rotation of angleDeg (0 / 90 / 180 / 270) using QTransform.
QImage applyRotation(const QImage& image, int angleDeg);

} // namespace mobile::rotate
