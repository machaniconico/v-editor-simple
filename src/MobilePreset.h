#pragma once
#include <QSize>
#include "MobileDeviceProfile.h"
#include "ExportDialog.h"

namespace mobile::preset {

// Build a fully-resolved ExportConfig for a given device profile.
// sourceSize      : original video dimensions (width x height)
// measuredLufs    : integrated loudness of the source audio (e.g. -14.0)
ExportConfig configForDevice(const DeviceProfile& device,
                             const QSize&         sourceSize,
                             double               measuredLufs);

} // namespace mobile::preset
