#include "ColorMatchLutGenerator.h"

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace colormatch::lut {

Lut3D generateMatchLut(const colormatch::analyze::ColorStats &source,
                       const colormatch::analyze::ColorStats &target,
                       int lutSize)
{
    Lut3D lut;
    lut.size = lutSize;
    lut.data.resize(lutSize * lutSize * lutSize);

    // Per-channel scale factors: target.std / max(source.std, 1e-6)
    // If source.std == 0, fall back to target.std (scale = target.std / target.std = 1.0,
    // unless target.std is also 0 in which case the image is constant and scale = 1.0).
    auto safeScale = [](double srcStd, double tgtStd) -> double {
        double denom = srcStd > 1e-6 ? srcStd : (tgtStd > 1e-6 ? tgtStd : 1.0);
        return tgtStd / denom;
    };

    double rScale = safeScale(source.rStd, target.rStd);
    double gScale = safeScale(source.gStd, target.gStd);
    double bScale = safeScale(source.bStd, target.bStd);

    auto clamp01 = [](double v) -> double {
        return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
    };

    // .cube index order: R varies fastest, then G, then B
    // index = r + g * size + b * size * size
    for (int b = 0; b < lutSize; ++b) {
        for (int g = 0; g < lutSize; ++g) {
            for (int r = 0; r < lutSize; ++r) {
                double inputR = r / (lutSize - 1.0);
                double inputG = g / (lutSize - 1.0);
                double inputB = b / (lutSize - 1.0);

                // Mean/std transfer in [0..255] space, then back to [0..1]
                double outR = (inputR * 255.0 - source.rMean) * rScale + target.rMean;
                double outG = (inputG * 255.0 - source.gMean) * gScale + target.gMean;
                double outB = (inputB * 255.0 - source.bMean) * bScale + target.bMean;

                int idx = r + g * lutSize + b * lutSize * lutSize;
                lut.data[idx] = QVector3D(
                    static_cast<float>(clamp01(outR / 255.0)),
                    static_cast<float>(clamp01(outG / 255.0)),
                    static_cast<float>(clamp01(outB / 255.0)));
            }
        }
    }

    return lut;
}

bool exportCube(const Lut3D &lut, const QString &path, const QString &title)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out.setRealNumberPrecision(6);
    out.setRealNumberNotation(QTextStream::FixedNotation);

    // Header
    out << "TITLE \"" << title << "\"\n";
    out << "LUT_3D_SIZE " << lut.size << "\n";
    out << "DOMAIN_MIN 0.0 0.0 0.0\n";
    out << "DOMAIN_MAX 1.0 1.0 1.0\n";

    // Data: one RGB triplet per line, 6 decimal places
    for (const QVector3D &v : lut.data) {
        out << v.x() << " " << v.y() << " " << v.z() << "\n";
    }

    return true;
}

} // namespace colormatch::lut
