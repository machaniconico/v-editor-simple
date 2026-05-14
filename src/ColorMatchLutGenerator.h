#pragma once

#include <QString>
#include <QVector>
#include <QVector3D>

#include "ColorMatchAnalyzer.h"

namespace colormatch::lut {

// 3D LUT data: size^3 entries, R varies fastest (matches .cube spec)
struct Lut3D {
    int size = 0;
    QVector<QVector3D> data;  // RGB triplets in [0.0, 1.0]
};

// Generate a color-matching LUT that transforms source color statistics
// to match the target color statistics via per-channel mean/std transfer.
// source.std == 0 falls back to using target.std (divide-by-zero guard).
Lut3D generateMatchLut(const colormatch::analyze::ColorStats &source,
                       const colormatch::analyze::ColorStats &target,
                       int lutSize = 33);

// Export a Lut3D to an Adobe .cube file.
// Returns true on success, false on file-open error.
bool exportCube(const Lut3D &lut, const QString &path,
                const QString &title = QStringLiteral("ColorMatchLUT"));

} // namespace colormatch::lut
