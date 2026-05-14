#include "MobileColorSpace.h"

#include <cmath>
#include <algorithm>

namespace mobile::color {

// ---------------------------------------------------------------------------
// D65 sRGB <-> Display P3 matrices.
//
// Derived from ITU-R BT.709 and SMPTE RP 431-2 / Apple Display P3 primaries
// with Bradford chromatic adaptation (D65 throughout — both spaces share the
// D65 white point so the adaptation is identity, but we use the Bradford-
// derived numerical form published by Color.org / Apple ColorSync).
//
// Reference values (8 sig figs):
//   sRGB -> P3: [[ 0.82246197,  0.17753803,  0.00000000],
//                [ 0.03319420,  0.96680580,  0.00000000],
//                [ 0.01708263,  0.07239744,  0.91051993]]
//   P3   -> sRGB: analytical inverse of the above.
// ---------------------------------------------------------------------------

Mat3x3 matrixSrgbToP3()
{
    return {{
        { 0.82246197, 0.17753803, 0.00000000 },
        { 0.03319420, 0.96680580, 0.00000000 },
        { 0.01708263, 0.07239744, 0.91051993 }
    }};
}

Mat3x3 matrixP3ToSrgb()
{
    // Analytical inverse of matrixSrgbToP3() (8 sig figs; satisfies
    // round-trip identity to <1e-6 off-diagonal).
    return {{
        {  1.22494018, -0.22494018,  0.00000000 },
        { -0.04205695,  1.04205695,  0.00000000 },
        { -0.01963755, -0.07863605,  1.09827360 }
    }};
}

Mat3x3 multiply(const Mat3x3& a, const Mat3x3& b)
{
    Mat3x3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 3; ++k) sum += a.m[i][k] * b.m[k][j];
            r.m[i][j] = sum;
        }
    }
    return r;
}

// ---------------------------------------------------------------------------
// sRGB transfer function (IEC 61966-2-1). Display P3 shares the same
// transfer, so we re-use these helpers for both spaces.
// ---------------------------------------------------------------------------
double srgbToLinear(double c)
{
    if (c <= 0.0) return 0.0;
    if (c >= 1.0) return 1.0;
    return (c <= 0.04045)
        ? (c / 12.92)
        : std::pow((c + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double c)
{
    if (c <= 0.0) return 0.0;
    if (c >= 1.0) return 1.0;
    return (c <= 0.0031308)
        ? (12.92 * c)
        : (1.055 * std::pow(c, 1.0 / 2.4) - 0.055);
}

namespace {

inline int clamp255(double x)
{
    if (x < 0.0)   return 0;
    if (x > 255.0) return 255;
    return static_cast<int>(std::lround(x));
}

QImage convertImage(const QImage& src, const Mat3x3& m)
{
    if (src.isNull()) return QImage();

    QImage img = src.convertToFormat(QImage::Format_ARGB32);
    const int w = img.width();
    const int h = img.height();

    for (int y = 0; y < h; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = row[x];
            const int a = qAlpha(px);

            // Gamma-encoded -> linear.
            const double rL = srgbToLinear(qRed(px)   / 255.0);
            const double gL = srgbToLinear(qGreen(px) / 255.0);
            const double bL = srgbToLinear(qBlue(px)  / 255.0);

            // Linear matrix multiply.
            const double R = m.m[0][0] * rL + m.m[0][1] * gL + m.m[0][2] * bL;
            const double G = m.m[1][0] * rL + m.m[1][1] * gL + m.m[1][2] * bL;
            const double B = m.m[2][0] * rL + m.m[2][1] * gL + m.m[2][2] * bL;

            // Re-encode (clamp to display gamut).
            const double rOut = linearToSrgb(std::clamp(R, 0.0, 1.0));
            const double gOut = linearToSrgb(std::clamp(G, 0.0, 1.0));
            const double bOut = linearToSrgb(std::clamp(B, 0.0, 1.0));

            row[x] = qRgba(clamp255(rOut * 255.0),
                           clamp255(gOut * 255.0),
                           clamp255(bOut * 255.0),
                           a);
        }
    }
    return img;
}

} // namespace

QImage convertSrgbToP3(const QImage& src)
{
    return convertImage(src, matrixSrgbToP3());
}

QImage convertP3ToSrgb(const QImage& src)
{
    return convertImage(src, matrixP3ToSrgb());
}

} // namespace mobile::color
