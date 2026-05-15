#pragma once

#include <QColor>
#include <QImage>

namespace chromakey {

struct KeyConfig {
    QColor keyColor      = QColor(0, 177, 64); // default: green-screen green
    double similarity    = 0.4;                 // 0.0-1.0 HSV-distance threshold
    double smoothness    = 0.1;                 // transition width around threshold
    double spillSuppress = 0.5;                 // spill-suppression strength 0.0-1.0
};

// HSV-distance keying.
// Returns alpha in [0, 1]: 0.0 = fully keyed out (matches keyColor), 1.0 = opaque.
double computeAlpha(const QColor &px, const KeyConfig &cfg);

// Reduce spill of the key channel toward the average of the other two channels.
// Operates on the RGB colour, leaving alpha unchanged.
QColor despill(const QColor &px, const KeyConfig &cfg);

// Return Format_ARGB32 image: despilled RGB with alpha from computeAlpha.
// Edge feathering is provided by the smoothstep in computeAlpha.
QImage refineMatte(const QImage &source, const KeyConfig &cfg);

} // namespace chromakey
