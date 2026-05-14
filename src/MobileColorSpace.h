#pragma once
#include <QImage>

namespace mobile::color {

// Row-major 3x3 matrix (m[row][col]).
struct Mat3x3 {
    double m[3][3];
};

// D65 sRGB <-> Display P3, Bradford-adapted (no white-point shift).
Mat3x3 matrixSrgbToP3();
Mat3x3 matrixP3ToSrgb();

// Multiply two 3x3 matrices: out = a * b.
Mat3x3 multiply(const Mat3x3& a, const Mat3x3& b);

// sRGB transfer (IEC 61966-2-1) — operates on normalized [0..1] components.
double srgbToLinear(double c);
double linearToSrgb(double c);

// Convert an entire QImage between gamma-encoded sRGB and Display P3.
// Output keeps the same Format_ARGB32 layout (alpha is preserved).
QImage convertSrgbToP3(const QImage& src);
QImage convertP3ToSrgb(const QImage& src);

} // namespace mobile::color
