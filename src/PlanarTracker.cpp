#include "PlanarTracker.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

// ===========================================================================
// namespace planartrack  — original Lucas-Kanade tracker implementation
// ===========================================================================

namespace planartrack {

namespace {

constexpr int kReducedPyramidLevels = 3;
constexpr int kMaxIterationsPerLevel = 30;
constexpr double kInitialLambda = 1e-3;
constexpr double kEarlyTerminateInfNorm = 1e-4;
constexpr double kResidualFailureThreshold = 30.0;
constexpr double kConfidenceFailureThreshold = 0.3;
constexpr double kTukeySigmaFromMad = 1.4826;

using Matrix8 = std::array<std::array<double, 8>, 8>;
using Vector8 = std::array<double, 8>;

struct EvaluationData {
    std::vector<int> validIndices;
    std::vector<double> warpedValues;
    std::vector<double> residuals;
    std::vector<double> weights;
    double objective = std::numeric_limits<double>::infinity();
    double rms = 255.0;
    double tukeyC = 0.0;
};

inline Homography identityHomography()
{
    return {1.0, 0.0, 0.0,
            0.0, 1.0, 0.0,
            0.0, 0.0, 1.0};
}

bool isValidImage(const LumaImage& image)
{
    if (image.width <= 0 || image.height <= 0) {
        return false;
    }
    const int stride = image.stride > 0 ? image.stride : image.width;
    const std::size_t needed = static_cast<std::size_t>(stride) * static_cast<std::size_t>(image.height);
    return image.pixels.size() >= needed;
}

LumaImage normalizeImage(const LumaImage& image)
{
    LumaImage normalized = image;
    if (normalized.stride <= 0) {
        normalized.stride = normalized.width;
    }
    return normalized;
}

double clampDouble(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

double imageValueAt(const LumaImage& image, int x, int y)
{
    x = std::max(0, std::min(image.width - 1, x));
    y = std::max(0, std::min(image.height - 1, y));
    return static_cast<double>(image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.stride) + static_cast<std::size_t>(x)]);
}

double bilinearSample(const LumaImage& image, double x, double y, bool* valid = nullptr)
{
    const bool inside = x >= 0.0 && y >= 0.0
        && x <= static_cast<double>(image.width - 1)
        && y <= static_cast<double>(image.height - 1);
    if (valid) {
        *valid = inside;
    }
    if (!inside) {
        return 0.0;
    }

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = std::min(x0 + 1, image.width - 1);
    const int y1 = std::min(y0 + 1, image.height - 1);
    const double fx = x - static_cast<double>(x0);
    const double fy = y - static_cast<double>(y0);

    const double v00 = imageValueAt(image, x0, y0);
    const double v10 = imageValueAt(image, x1, y0);
    const double v01 = imageValueAt(image, x0, y1);
    const double v11 = imageValueAt(image, x1, y1);

    const double top = v00 + (v10 - v00) * fx;
    const double bottom = v01 + (v11 - v01) * fx;
    return top + (bottom - top) * fy;
}

double gradientX(const LumaImage& image, double x, double y)
{
    return 0.5 * (bilinearSample(image, x + 1.0, y) - bilinearSample(image, x - 1.0, y));
}

double gradientY(const LumaImage& image, double x, double y)
{
    return 0.5 * (bilinearSample(image, x, y + 1.0) - bilinearSample(image, x, y - 1.0));
}

Quad scaleQuad(const Quad& quad, double factor)
{
    Quad scaled = quad;
    scaled.tl.x *= factor;
    scaled.tl.y *= factor;
    scaled.tr.x *= factor;
    scaled.tr.y *= factor;
    scaled.br.x *= factor;
    scaled.br.y *= factor;
    scaled.bl.x *= factor;
    scaled.bl.y *= factor;
    return scaled;
}

double edgeCross(double ax, double ay, double bx, double by, double px, double py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

bool pointInsideQuad(const Quad& quad, double x, double y)
{
    const std::array<Point2D, 4> corners = {quad.tl, quad.tr, quad.br, quad.bl};
    double sign = 0.0;
    for (int i = 0; i < 4; ++i) {
        const Point2D& a = corners[static_cast<std::size_t>(i)];
        const Point2D& b = corners[static_cast<std::size_t>((i + 1) % 4)];
        const double cross = edgeCross(a.x, a.y, b.x, b.y, x, y);
        if (std::abs(cross) < 1e-9) {
            continue;
        }
        if (sign == 0.0) {
            sign = cross;
            continue;
        }
        if ((sign > 0.0) != (cross > 0.0)) {
            return false;
        }
    }
    return true;
}

LumaImage downsampleGaussian5Tap(const LumaImage& source)
{
    static constexpr std::array<double, 5> kernel = {1.0, 4.0, 6.0, 4.0, 1.0};
    const int sourceStride = source.stride > 0 ? source.stride : source.width;

    std::vector<double> horizontal(static_cast<std::size_t>(source.width) * static_cast<std::size_t>(source.height), 0.0);
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            double sum = 0.0;
            for (int k = -2; k <= 2; ++k) {
                const int sampleX = std::max(0, std::min(source.width - 1, x + k));
                sum += kernel[static_cast<std::size_t>(k + 2)]
                    * static_cast<double>(source.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(sourceStride) + static_cast<std::size_t>(sampleX)]);
            }
            horizontal[static_cast<std::size_t>(y) * static_cast<std::size_t>(source.width) + static_cast<std::size_t>(x)] = sum / 16.0;
        }
    }

    const int outWidth = std::max(1, (source.width + 1) / 2);
    const int outHeight = std::max(1, (source.height + 1) / 2);
    LumaImage output(outWidth, outHeight);

    for (int y = 0; y < outHeight; ++y) {
        const int sourceY = std::min(source.height - 1, y * 2);
        for (int x = 0; x < outWidth; ++x) {
            const int sourceX = std::min(source.width - 1, x * 2);
            double sum = 0.0;
            for (int k = -2; k <= 2; ++k) {
                const int sampleY = std::max(0, std::min(source.height - 1, sourceY + k));
                sum += kernel[static_cast<std::size_t>(k + 2)]
                    * horizontal[static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(source.width) + static_cast<std::size_t>(sourceX)];
            }
            const double blurred = sum / 16.0;
            const double rounded = clampDouble(std::round(blurred), 0.0, 255.0);
            output.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(output.stride) + static_cast<std::size_t>(x)] =
                static_cast<std::uint8_t>(rounded);
        }
    }

    return output;
}

std::vector<LumaImage> buildPyramid(const LumaImage& baseImage)
{
    std::vector<LumaImage> pyramid;
    pyramid.reserve(static_cast<std::size_t>(kReducedPyramidLevels + 1));
    pyramid.push_back(baseImage);
    for (int level = 0; level < kReducedPyramidLevels; ++level) {
        pyramid.push_back(downsampleGaussian5Tap(pyramid.back()));
    }
    return pyramid;
}

double medianOf(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }

    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    double median = values[middle];
    if ((values.size() % 2U) == 0U) {
        std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle - 1), values.begin() + static_cast<std::ptrdiff_t>(middle + 1));
        median = 0.5 * (median + values[middle - 1]);
    }
    return median;
}

double tukeyWeight(double residual, double c)
{
    if (c <= 1e-12) {
        return 1.0;
    }
    const double scaled = residual / c;
    const double absScaled = std::abs(scaled);
    if (absScaled >= 1.0) {
        return 0.0;
    }
    const double oneMinus = 1.0 - scaled * scaled;
    return oneMinus * oneMinus;
}

Homography normalizeHomography(Homography H)
{
    const double denom = std::abs(H[8]) > 1e-12 ? H[8] : 1.0;
    for (double& value : H) {
        value /= denom;
    }
    H[8] = 1.0;
    return H;
}

void warpPoint(const Homography& H, double x, double y, double& outX, double& outY, double& w)
{
    const double X = H[0] * x + H[1] * y + H[2];
    const double Y = H[3] * x + H[4] * y + H[5];
    w = H[6] * x + H[7] * y + 1.0;
    if (std::abs(w) < 1e-12) {
        outX = X;
        outY = Y;
        return;
    }
    outX = X / w;
    outY = Y / w;
}

void addDeltaToHomography(Homography& H, const Vector8& delta)
{
    H[0] += delta[0];
    H[1] += delta[1];
    H[2] += delta[2];
    H[3] += delta[3];
    H[4] += delta[4];
    H[5] += delta[5];
    H[6] += delta[6];
    H[7] += delta[7];
    H[8] = 1.0;
    H = normalizeHomography(H);
}

void computeSteepestDescentRow(const PlanarTracker::Sample& sample, const Homography& H, Vector8& J)
{
    double warpedX = 0.0;
    double warpedY = 0.0;
    double w = 1.0;
    warpPoint(H, sample.x, sample.y, warpedX, warpedY, w);
    if (std::abs(w) < 1e-12) {
        J.fill(0.0);
        return;
    }

    const double invW = 1.0 / w;
    const double dxdh20 = -sample.x * warpedX * invW;
    const double dxdh21 = -sample.y * warpedX * invW;
    const double dydh20 = -sample.x * warpedY * invW;
    const double dydh21 = -sample.y * warpedY * invW;

    J[0] = sample.gradX * sample.x * invW;
    J[1] = sample.gradX * sample.y * invW;
    J[2] = sample.gradX * invW;
    J[3] = sample.gradY * sample.x * invW;
    J[4] = sample.gradY * sample.y * invW;
    J[5] = sample.gradY * invW;
    J[6] = sample.gradX * dxdh20 + sample.gradY * dydh20;
    J[7] = sample.gradX * dxdh21 + sample.gradY * dydh21;
}

bool solveLinearSystem(Matrix8 A, Vector8 b, Vector8& x)
{
    for (int pivot = 0; pivot < 8; ++pivot) {
        int maxRow = pivot;
        double maxAbs = std::abs(A[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(pivot)]);
        for (int row = pivot + 1; row < 8; ++row) {
            const double candidate = std::abs(A[static_cast<std::size_t>(row)][static_cast<std::size_t>(pivot)]);
            if (candidate > maxAbs) {
                maxAbs = candidate;
                maxRow = row;
            }
        }
        if (maxAbs < 1e-12) {
            return false;
        }
        if (maxRow != pivot) {
            std::swap(A[static_cast<std::size_t>(pivot)], A[static_cast<std::size_t>(maxRow)]);
            std::swap(b[static_cast<std::size_t>(pivot)], b[static_cast<std::size_t>(maxRow)]);
        }

        const double diag = A[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(pivot)];
        for (int col = pivot; col < 8; ++col) {
            A[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(col)] /= diag;
        }
        b[static_cast<std::size_t>(pivot)] /= diag;

        for (int row = 0; row < 8; ++row) {
            if (row == pivot) {
                continue;
            }
            const double factor = A[static_cast<std::size_t>(row)][static_cast<std::size_t>(pivot)];
            if (std::abs(factor) < 1e-18) {
                continue;
            }
            for (int col = pivot; col < 8; ++col) {
                A[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] -=
                    factor * A[static_cast<std::size_t>(pivot)][static_cast<std::size_t>(col)];
            }
            b[static_cast<std::size_t>(row)] -= factor * b[static_cast<std::size_t>(pivot)];
        }
    }

    x = b;
    return true;
}

double maxAbs(const Vector8& values)
{
    double magnitude = 0.0;
    for (double value : values) {
        magnitude = std::max(magnitude, std::abs(value));
    }
    return magnitude;
}

Homography toPyramidLevel(Homography H, int divisor)
{
    if (divisor <= 1) {
        return normalizeHomography(H);
    }

    const double d = static_cast<double>(divisor);
    H[2] /= d;
    H[5] /= d;
    H[6] *= d;
    H[7] *= d;
    return normalizeHomography(H);
}

Homography upsampleToFinerLevel(Homography H)
{
    H[2] *= 2.0;
    H[5] *= 2.0;
    H[6] *= 0.5;
    H[7] *= 0.5;
    return normalizeHomography(H);
}

EvaluationData evaluateWarp(const PlanarTracker::TemplateLevel& level, const LumaImage& frame, const Homography& H)
{
    EvaluationData evaluation;
    evaluation.warpedValues.assign(level.samples.size(), 0.0);
    evaluation.residuals.assign(level.samples.size(), 0.0);
    evaluation.weights.assign(level.samples.size(), 0.0);

    double warpedSum = 0.0;
    for (std::size_t index = 0; index < level.samples.size(); ++index) {
        const PlanarTracker::Sample& sample = level.samples[index];
        double warpedX = 0.0;
        double warpedY = 0.0;
        double w = 1.0;
        warpPoint(H, sample.x, sample.y, warpedX, warpedY, w);
        bool valid = false;
        const double warpedValue = bilinearSample(frame, warpedX, warpedY, &valid);
        if (!valid || !std::isfinite(warpedValue) || !std::isfinite(w) || std::abs(w) < 1e-12) {
            continue;
        }
        evaluation.validIndices.push_back(static_cast<int>(index));
        evaluation.warpedValues[index] = warpedValue;
        warpedSum += warpedValue;
    }

    if (evaluation.validIndices.size() < 8U) {
        evaluation.objective = std::numeric_limits<double>::infinity();
        evaluation.rms = 255.0;
        return evaluation;
    }

    const double warpedMean = warpedSum / static_cast<double>(evaluation.validIndices.size());

    std::vector<double> validResiduals;
    validResiduals.reserve(evaluation.validIndices.size());
    double squaredResidualSum = 0.0;
    for (int rawIndex : evaluation.validIndices) {
        const std::size_t index = static_cast<std::size_t>(rawIndex);
        const double residual = (evaluation.warpedValues[index] - warpedMean) - level.samples[index].referenceZeroMean;
        evaluation.residuals[index] = residual;
        squaredResidualSum += residual * residual;
        validResiduals.push_back(residual);
    }

    const double residualMedian = medianOf(validResiduals);
    std::vector<double> deviations;
    deviations.reserve(validResiduals.size());
    for (double residual : validResiduals) {
        deviations.push_back(std::abs(residual - residualMedian));
    }
    const double mad = medianOf(std::move(deviations));
    const double sigma = mad > 1e-9 ? (kTukeySigmaFromMad * mad) : 0.0;
    evaluation.tukeyC = sigma > 1e-9 ? (4.0 * sigma) : 0.0;

    evaluation.objective = 0.0;
    for (int rawIndex : evaluation.validIndices) {
        const std::size_t index = static_cast<std::size_t>(rawIndex);
        const double residual = evaluation.residuals[index];
        const double weight = evaluation.tukeyC > 0.0 ? tukeyWeight(residual, evaluation.tukeyC) : 1.0;
        evaluation.weights[index] = weight;
        evaluation.objective += weight * residual * residual;
    }
    evaluation.rms = std::sqrt(squaredResidualSum / static_cast<double>(evaluation.validIndices.size()));
    return evaluation;
}

bool refineAtLevel(const PlanarTracker::TemplateLevel& level, const LumaImage& frame, Homography& H)
{
    double lambda = kInitialLambda;
    EvaluationData current = evaluateWarp(level, frame, H);
    if (!std::isfinite(current.objective)) {
        return false;
    }

    for (int iteration = 0; iteration < kMaxIterationsPerLevel; ++iteration) {
        Matrix8 normal{};
        Vector8 rhs{};
        Vector8 diagonal{};

        for (int rawIndex : current.validIndices) {
            const std::size_t index = static_cast<std::size_t>(rawIndex);
            const double weight = current.weights[index];
            if (weight <= 0.0) {
                continue;
            }

            Vector8 J{};
            computeSteepestDescentRow(level.samples[index], H, J);
            for (int row = 0; row < 8; ++row) {
                rhs[static_cast<std::size_t>(row)] += -weight * J[static_cast<std::size_t>(row)] * current.residuals[index];
                diagonal[static_cast<std::size_t>(row)] += weight * J[static_cast<std::size_t>(row)] * J[static_cast<std::size_t>(row)];
                for (int col = row; col < 8; ++col) {
                    normal[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] +=
                        weight * J[static_cast<std::size_t>(row)] * J[static_cast<std::size_t>(col)];
                }
            }
        }

        for (int row = 0; row < 8; ++row) {
            for (int col = row + 1; col < 8; ++col) {
                normal[static_cast<std::size_t>(col)][static_cast<std::size_t>(row)] =
                    normal[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
            }
            normal[static_cast<std::size_t>(row)][static_cast<std::size_t>(row)] +=
                lambda * (diagonal[static_cast<std::size_t>(row)] + 1e-9);
        }

        Vector8 delta{};
        if (!solveLinearSystem(normal, rhs, delta)) {
            return false;
        }

        Homography candidate = H;
        addDeltaToHomography(candidate, delta);
        EvaluationData trial = evaluateWarp(level, frame, candidate);
        if (std::isfinite(trial.objective) && trial.objective <= current.objective) {
            H = candidate;
            current = std::move(trial);
            lambda = std::max(1e-6, lambda * 0.5);
            if (maxAbs(delta) < kEarlyTerminateInfNorm) {
                return true;
            }
        } else {
            lambda = std::min(1e6, lambda * 4.0);
        }
    }

    return true;
}

PlanarTracker::TemplateLevel buildTemplateLevel(const LumaImage& referenceLevel, const Quad& scaledQuad, int scaleDivisor)
{
    PlanarTracker::TemplateLevel level;
    level.quad = scaledQuad;
    level.scaleDivisor = scaleDivisor;

    const double minX = std::min(std::min(scaledQuad.tl.x, scaledQuad.tr.x), std::min(scaledQuad.br.x, scaledQuad.bl.x));
    const double maxX = std::max(std::max(scaledQuad.tl.x, scaledQuad.tr.x), std::max(scaledQuad.br.x, scaledQuad.bl.x));
    const double minY = std::min(std::min(scaledQuad.tl.y, scaledQuad.tr.y), std::min(scaledQuad.br.y, scaledQuad.bl.y));
    const double maxY = std::max(std::max(scaledQuad.tl.y, scaledQuad.tr.y), std::max(scaledQuad.br.y, scaledQuad.bl.y));

    const int startX = std::max(0, static_cast<int>(std::floor(minX)));
    const int endX = std::min(referenceLevel.width - 1, static_cast<int>(std::ceil(maxX)));
    const int startY = std::max(0, static_cast<int>(std::floor(minY)));
    const int endY = std::min(referenceLevel.height - 1, static_cast<int>(std::ceil(maxY)));

    level.samples.reserve(static_cast<std::size_t>(std::max(0, endX - startX + 1) * std::max(0, endY - startY + 1)));
    double sum = 0.0;
    for (int y = startY; y <= endY; ++y) {
        for (int x = startX; x <= endX; ++x) {
            if (!pointInsideQuad(scaledQuad, static_cast<double>(x), static_cast<double>(y))) {
                continue;
            }
            PlanarTracker::Sample sample;
            sample.x = static_cast<double>(x);
            sample.y = static_cast<double>(y);
            sample.reference = bilinearSample(referenceLevel, sample.x, sample.y);
            sample.gradX = gradientX(referenceLevel, sample.x, sample.y);
            sample.gradY = gradientY(referenceLevel, sample.x, sample.y);
            sum += sample.reference;
            level.samples.push_back(sample);
        }
    }

    if (!level.samples.empty()) {
        level.referenceMean = sum / static_cast<double>(level.samples.size());
        for (PlanarTracker::Sample& sample : level.samples) {
            sample.referenceZeroMean = sample.reference - level.referenceMean;
        }
    }

    return level;
}

FrameResult makeFailureResult(int frameIndex, const Homography& H)
{
    FrameResult result;
    result.frameIndex = frameIndex;
    result.H = H;
    result.residual = 255.0;
    result.confidence = 0.0;
    result.uncertain = true;
    return result;
}

} // namespace (anonymous)

bool isIdentityHomography(const Homography& H, double tolerance)
{
    static constexpr Homography identity = {1.0, 0.0, 0.0,
                                             0.0, 1.0, 0.0,
                                             0.0, 0.0, 1.0};
    for (std::size_t i = 0; i < 9; ++i) {
        if (std::abs(H[i] - identity[i]) > tolerance) {
            return false;
        }
    }
    return true;
}

bool shouldSkipOnSave(const PlanarTrack& track)
{
    if (track.frames.empty()) {
        return true;
    }
    for (const auto& frame : track.frames) {
        if (!isIdentityHomography(frame.H, 1e-12)) {
            return false;
        }
    }
    return true;
}

QJsonObject sourceClipKeyToJson(const SourceClipKey& key)
{
    QJsonObject obj;
    obj["filePath"] = key.filePath;
    obj["sourceTrack"] = key.sourceTrack;
    obj["sourceClipIndex"] = key.sourceClipIndex;
    return obj;
}

SourceClipKey sourceClipKeyFromJson(const QJsonObject& obj)
{
    SourceClipKey key;
    key.filePath = obj["filePath"].toString();
    key.sourceTrack = obj["sourceTrack"].toInt(0);
    key.sourceClipIndex = obj["sourceClipIndex"].toInt(0);
    return key;
}

QJsonObject quadToJson(const Quad& quad)
{
    QJsonObject obj;
    obj["tlX"] = quad.tl.x;
    obj["tlY"] = quad.tl.y;
    obj["trX"] = quad.tr.x;
    obj["trY"] = quad.tr.y;
    obj["brX"] = quad.br.x;
    obj["brY"] = quad.br.y;
    obj["blX"] = quad.bl.x;
    obj["blY"] = quad.bl.y;
    return obj;
}

Quad quadFromJson(const QJsonObject& obj)
{
    Quad quad;
    quad.tl.x = obj["tlX"].toDouble(0.0);
    quad.tl.y = obj["tlY"].toDouble(0.0);
    quad.tr.x = obj["trX"].toDouble(0.0);
    quad.tr.y = obj["trY"].toDouble(0.0);
    quad.br.x = obj["brX"].toDouble(0.0);
    quad.br.y = obj["brY"].toDouble(0.0);
    quad.bl.x = obj["blX"].toDouble(0.0);
    quad.bl.y = obj["blY"].toDouble(0.0);
    return quad;
}

QJsonObject frameResultToJson(const FrameResult& frame)
{
    QJsonObject obj;
    obj["frameIndex"] = frame.frameIndex;
    QJsonArray hArr;
    for (double v : frame.H) {
        hArr.append(v);
    }
    obj["H"] = hArr;
    obj["residual"] = frame.residual;
    obj["confidence"] = frame.confidence;
    obj["uncertain"] = frame.uncertain;
    return obj;
}

FrameResult frameResultFromJson(const QJsonObject& obj)
{
    FrameResult frame;
    frame.frameIndex = obj["frameIndex"].toInt(0);
    const QJsonArray hArr = obj["H"].toArray();
    for (int i = 0; i < 9 && i < hArr.size(); ++i) {
        frame.H[static_cast<std::size_t>(i)] = hArr[i].toDouble();
    }
    frame.residual = obj["residual"].toDouble(255.0);
    frame.confidence = obj["confidence"].toDouble(0.0);
    frame.uncertain = obj["uncertain"].toBool(true);
    return frame;
}

QJsonObject toJson(const PlanarTrack& track)
{
    QJsonObject obj;
    obj["trackId"] = track.trackId;
    obj["name"] = track.name;
    obj["sourceClipKey"] = sourceClipKeyToJson(track.sourceClipKey);
    obj["refFrameIndex"] = track.refFrameIndex;
    obj["refQuad"] = quadToJson(track.refQuad);
    QJsonArray framesArr;
    for (const auto& frame : track.frames) {
        framesArr.append(frameResultToJson(frame));
    }
    obj["frames"] = framesArr;
    return obj;
}

bool fromJson(const QJsonObject& obj, PlanarTrack& track)
{
    if (!obj.contains("trackId") || !obj.contains("name")) {
        return false;
    }
    track.trackId = obj["trackId"].toString();
    track.name = obj["name"].toString();

    if (obj.contains("sourceClipKey")) {
        track.sourceClipKey = sourceClipKeyFromJson(obj["sourceClipKey"].toObject());
    }

    track.refFrameIndex = obj["refFrameIndex"].toInt(0);

    if (obj.contains("refQuad")) {
        track.refQuad = quadFromJson(obj["refQuad"].toObject());
    }

    track.frames.clear();
    if (obj.contains("frames")) {
        const QJsonArray framesArr = obj["frames"].toArray();
        track.frames.reserve(static_cast<std::size_t>(framesArr.size()));
        for (const auto& val : framesArr) {
            track.frames.push_back(frameResultFromJson(val.toObject()));
        }
    }

    return true;
}

PlanarTracker::PlanarTracker(const LumaImage& ref, const Quad& refQuad)
{
    LumaImage normalized = normalizeImage(ref);
    if (!isValidImage(normalized)) {
        return;
    }
    m_referencePyramid = buildPyramid(normalized);
    m_templates.reserve(m_referencePyramid.size());
    for (std::size_t level = 0; level < m_referencePyramid.size(); ++level) {
        const int divisor = 1 << static_cast<int>(level);
        const Quad scaledQuad = scaleQuad(refQuad, 1.0 / static_cast<double>(divisor));
        m_templates.push_back(buildTemplateLevel(m_referencePyramid[level], scaledQuad, divisor));
    }
}

FrameResult PlanarTracker::track(const LumaImage& frame, int frameIndex)
{
    LumaImage normalized = normalizeImage(frame);
    if (!isValidImage(normalized) || m_referencePyramid.empty()) {
        return makeFailureResult(frameIndex, m_lastH);
    }

    const std::vector<LumaImage> framePyramid = buildPyramid(normalized);
    Homography H = m_hasLastH ? m_lastH : identityHomography();

    const int topLevel = static_cast<int>(m_templates.size()) - 1;
    H = toPyramidLevel(H, m_templates[static_cast<std::size_t>(topLevel)].scaleDivisor);

    for (int level = topLevel; level >= 0; --level) {
        const std::size_t idx = static_cast<std::size_t>(level);
        if (idx < framePyramid.size() && idx < m_templates.size()) {
            refineAtLevel(m_templates[idx], framePyramid[idx], H);
        }
        if (level > 0) {
            H = upsampleToFinerLevel(H);
        }
    }

    H = normalizeHomography(H);

    EvaluationData eval = evaluateWarp(m_templates[0], framePyramid[0], H);
    const double residual = eval.rms;
    const double confidence = residual < kResidualFailureThreshold
        ? std::max(0.0, 1.0 - residual / kResidualFailureThreshold)
        : 0.0;
    const bool uncertain = confidence < kConfidenceFailureThreshold;

    if (!uncertain) {
        m_lastH = H;
        m_hasLastH = true;
    }

    FrameResult result;
    result.frameIndex = frameIndex;
    result.H = H;
    result.residual = residual;
    result.confidence = confidence;
    result.uncertain = uncertain;
    return result;
}

#ifdef VEDITOR_PLANAR_TRACKER_SELFTEST
namespace tests {

namespace {

LumaImage makeCheckerPatchFrame(int width, int height, int patchX, int patchY, int patchSize, int cellSize)
{
    LumaImage image(width, height);
    for (int y = 0; y < patchSize; ++y) {
        for (int x = 0; x < patchSize; ++x) {
            const int tileX = x / cellSize;
            const int tileY = y / cellSize;
            const bool white = ((tileX + tileY) % 2) == 0;
            image.pixels[static_cast<std::size_t>(patchY + y) * static_cast<std::size_t>(image.stride) + static_cast<std::size_t>(patchX + x)] =
                white ? 255U : 32U;
        }
    }
    return image;
}

LumaImage translateFrame(const LumaImage& source, int dx, int dy)
{
    LumaImage translated(source.width, source.height);
    for (int y = 0; y < source.height; ++y) {
        for (int x = 0; x < source.width; ++x) {
            const int sourceX = x - dx;
            const int sourceY = y - dy;
            std::uint8_t value = 0;
            if (sourceX >= 0 && sourceX < source.width && sourceY >= 0 && sourceY < source.height) {
                value = source.pixels[static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(source.stride) + static_cast<std::size_t>(sourceX)];
            }
            translated.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(translated.stride) + static_cast<std::size_t>(x)] = value;
        }
    }
    return translated;
}

Point2D mapPoint(const Homography& H, const Point2D& point)
{
    double outX = 0.0;
    double outY = 0.0;
    double w = 1.0;
    warpPoint(H, point.x, point.y, outX, outY, w);
    return {outX, outY};
}

bool nearlyEqual(double a, double b, double tolerance)
{
    return std::abs(a - b) <= tolerance;
}

} // namespace

bool runSelfTest()
{
    constexpr int frameWidth = 80;
    constexpr int frameHeight = 80;
    constexpr int patchSize = 64;
    constexpr int patchOriginX = 8;
    constexpr int patchOriginY = 8;
    constexpr int translateX = 5;

    const LumaImage reference = makeCheckerPatchFrame(frameWidth, frameHeight, patchOriginX, patchOriginY, patchSize, 8);
    const LumaImage translated = translateFrame(reference, translateX, 0);

    Quad quad;
    quad.tl = {static_cast<double>(patchOriginX), static_cast<double>(patchOriginY)};
    quad.tr = {static_cast<double>(patchOriginX + patchSize - 1), static_cast<double>(patchOriginY)};
    quad.br = {static_cast<double>(patchOriginX + patchSize - 1), static_cast<double>(patchOriginY + patchSize - 1)};
    quad.bl = {static_cast<double>(patchOriginX), static_cast<double>(patchOriginY + patchSize - 1)};

    PlanarTracker tracker(reference, quad);
    const FrameResult result = tracker.track(translated, 1);

    const std::array<Point2D, 4> referenceCorners = {quad.tl, quad.tr, quad.br, quad.bl};
    for (const Point2D& corner : referenceCorners) {
        const Point2D mapped = mapPoint(result.H, corner);
        if (!nearlyEqual(mapped.x, corner.x + static_cast<double>(translateX), 0.1)
            || !nearlyEqual(mapped.y, corner.y, 0.1)) {
            return false;
        }
    }

    return result.residual < 5.0 && result.confidence > 0.9;
}

} // namespace tests
#endif

} // namespace planartrack

// ===========================================================================
// namespace planar  — SAD-based 4-corner planar tracker (Sprint 15, A1)
// ===========================================================================

namespace planar {

// ---------------------------------------------------------------------------
// CornerSet
// ---------------------------------------------------------------------------

static double cross2D(const QPointF& o, const QPointF& a, const QPointF& b)
{
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
}

bool CornerSet::isValid() const
{
    if (tl == tr && tr == br && br == bl)
        return false;

    const QPointF pts[4] = {tl, tr, br, bl};
    double sign = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double c = cross2D(pts[i], pts[(i + 1) % 4], pts[(i + 2) % 4]);
        if (std::abs(c) < 1e-9)
            continue;
        if (sign == 0.0) {
            sign = c;
        } else if ((sign > 0.0) != (c > 0.0)) {
            return false;
        }
    }
    return sign != 0.0;
}

QPointF CornerSet::center() const
{
    return (tl + tr + br + bl) / 4.0;
}

CornerSet CornerSet::rectangle(const QRectF& rect)
{
    CornerSet cs;
    cs.tl = rect.topLeft();
    cs.tr = rect.topRight();
    cs.br = rect.bottomRight();
    cs.bl = rect.bottomLeft();
    return cs;
}

// ---------------------------------------------------------------------------
// Homography
// ---------------------------------------------------------------------------

static bool gaussianElim8(double A[8][8], double b[8], double x[8])
{
    for (int pivot = 0; pivot < 8; ++pivot) {
        int maxRow = pivot;
        double maxVal = std::abs(A[pivot][pivot]);
        for (int row = pivot + 1; row < 8; ++row) {
            if (std::abs(A[row][pivot]) > maxVal) {
                maxVal = std::abs(A[row][pivot]);
                maxRow = row;
            }
        }
        if (maxVal < 1e-12)
            return false;

        if (maxRow != pivot) {
            for (int col = 0; col < 8; ++col)
                std::swap(A[pivot][col], A[maxRow][col]);
            std::swap(b[pivot], b[maxRow]);
        }

        const double diag = A[pivot][pivot];
        for (int col = pivot; col < 8; ++col)
            A[pivot][col] /= diag;
        b[pivot] /= diag;

        for (int row = 0; row < 8; ++row) {
            if (row == pivot)
                continue;
            const double factor = A[row][pivot];
            if (std::abs(factor) < 1e-18)
                continue;
            for (int col = pivot; col < 8; ++col)
                A[row][col] -= factor * A[pivot][col];
            b[row] -= factor * b[pivot];
        }
    }
    for (int i = 0; i < 8; ++i)
        x[i] = b[i];
    return true;
}

Homography homographyFromCorners(const CornerSet& src, const CornerSet& dst)
{
    const QPointF srcPts[4] = {src.tl, src.tr, src.br, src.bl};
    const QPointF dstPts[4] = {dst.tl, dst.tr, dst.br, dst.bl};

    double A[8][8] = {};
    double b[8] = {};

    for (int i = 0; i < 4; ++i) {
        const double xs = srcPts[i].x();
        const double ys = srcPts[i].y();
        const double xd = dstPts[i].x();
        const double yd = dstPts[i].y();

        const int r0 = i * 2;
        const int r1 = r0 + 1;

        A[r0][0] = xs;  A[r0][1] = ys;  A[r0][2] = 1.0;
        A[r0][3] = 0.0; A[r0][4] = 0.0; A[r0][5] = 0.0;
        A[r0][6] = -xd * xs;
        A[r0][7] = -xd * ys;
        b[r0] = xd;

        A[r1][0] = 0.0; A[r1][1] = 0.0; A[r1][2] = 0.0;
        A[r1][3] = xs;  A[r1][4] = ys;  A[r1][5] = 1.0;
        A[r1][6] = -yd * xs;
        A[r1][7] = -yd * ys;
        b[r1] = yd;
    }

    double xSol[8] = {};
    Homography h;
    if (gaussianElim8(A, b, xSol)) {
        for (int i = 0; i < 8; ++i)
            h.m[i] = xSol[i];
        h.m[8] = 1.0;
    }
    return h;
}

QPointF transformPoint(const QPointF& pt, const Homography& h)
{
    const double x = pt.x();
    const double y = pt.y();
    const double w = h.m[6] * x + h.m[7] * y + h.m[8];
    if (std::abs(w) < 1e-10)
        return QPointF(0, 0);
    return QPointF((h.m[0] * x + h.m[1] * y + h.m[2]) / w,
                   (h.m[3] * x + h.m[4] * y + h.m[5]) / w);
}

static bool invert3x3(const double m[9], double out[9])
{
    const double c00 =  (m[4] * m[8] - m[5] * m[7]);
    const double c01 = -(m[3] * m[8] - m[5] * m[6]);
    const double c02 =  (m[3] * m[7] - m[4] * m[6]);
    const double det = m[0] * c00 + m[1] * c01 + m[2] * c02;
    if (std::abs(det) < 1e-12)
        return false;
    const double inv = 1.0 / det;
    out[0] = c00 * inv;
    out[1] = -(m[1] * m[8] - m[2] * m[7]) * inv;
    out[2] =  (m[1] * m[5] - m[2] * m[4]) * inv;
    out[3] = c01 * inv;
    out[4] =  (m[0] * m[8] - m[2] * m[6]) * inv;
    out[5] = -(m[0] * m[5] - m[2] * m[3]) * inv;
    out[6] = c02 * inv;
    out[7] = -(m[0] * m[7] - m[1] * m[6]) * inv;
    out[8] =  (m[0] * m[4] - m[1] * m[3]) * inv;
    return true;
}

QImage warpImage(const QImage& source, const Homography& h, const QSize& outSize)
{
    if (outSize.isEmpty() || source.isNull())
        return QImage();

    double inv[9];
    if (!invert3x3(h.m, inv))
        std::copy(h.m, h.m + 9, inv);

    const QImage src = source.convertToFormat(QImage::Format_ARGB32);
    QImage out(outSize, QImage::Format_ARGB32);
    out.fill(Qt::transparent);

    const int srcW = src.width();
    const int srcH = src.height();
    const int outW = outSize.width();
    const int outH = outSize.height();

    for (int oy = 0; oy < outH; ++oy) {
        QRgb* outLine = reinterpret_cast<QRgb*>(out.scanLine(oy));
        for (int ox = 0; ox < outW; ++ox) {
            const double fx = static_cast<double>(ox);
            const double fy = static_cast<double>(oy);
            const double w  = inv[6] * fx + inv[7] * fy + inv[8];
            if (std::abs(w) < 1e-10) {
                outLine[ox] = 0;
                continue;
            }
            const double sx = (inv[0] * fx + inv[1] * fy + inv[2]) / w;
            const double sy = (inv[3] * fx + inv[4] * fy + inv[5]) / w;

            if (sx < 0.0 || sy < 0.0 || sx > static_cast<double>(srcW - 1) || sy > static_cast<double>(srcH - 1)) {
                outLine[ox] = 0;
                continue;
            }

            const int x0 = static_cast<int>(std::floor(sx));
            const int y0 = static_cast<int>(std::floor(sy));
            const int x1 = std::min(x0 + 1, srcW - 1);
            const int y1 = std::min(y0 + 1, srcH - 1);
            const double fx2 = sx - static_cast<double>(x0);
            const double fy2 = sy - static_cast<double>(y0);

            const QRgb p00 = src.pixel(x0, y0);
            const QRgb p10 = src.pixel(x1, y0);
            const QRgb p01 = src.pixel(x0, y1);
            const QRgb p11 = src.pixel(x1, y1);

            const auto lerp = [](double a, double b2, double t) { return a + (b2 - a) * t; };
            const double r  = lerp(lerp(qRed(p00),   qRed(p10),   fx2), lerp(qRed(p01),   qRed(p11),   fx2), fy2);
            const double g  = lerp(lerp(qGreen(p00), qGreen(p10), fx2), lerp(qGreen(p01), qGreen(p11), fx2), fy2);
            const double bl = lerp(lerp(qBlue(p00),  qBlue(p10),  fx2), lerp(qBlue(p01),  qBlue(p11),  fx2), fy2);
            const double a  = lerp(lerp(qAlpha(p00), qAlpha(p10), fx2), lerp(qAlpha(p01), qAlpha(p11), fx2), fy2);

            outLine[ox] = qRgba(static_cast<int>(r + 0.5),
                                 static_cast<int>(g + 0.5),
                                 static_cast<int>(bl + 0.5),
                                 static_cast<int>(a + 0.5));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Tracker
// ---------------------------------------------------------------------------

Tracker::Tracker() = default;

void Tracker::setParams(const TrackParams& p)
{
    m_params = p;
}

TrackParams Tracker::params() const
{
    return m_params;
}

void Tracker::reset()
{
    m_initialized = false;
    m_refFrame = QImage();
    m_lastCorners = CornerSet();
    for (int i = 0; i < 4; ++i)
        m_patches[i] = QImage();
}

QImage Tracker::extractPatch(const QImage& image, const QPointF& center, double size) const
{
    const int half = static_cast<int>(size / 2.0);
    const int patchW = static_cast<int>(size);
    const int patchH = static_cast<int>(size);
    const int cx = static_cast<int>(std::round(center.x()));
    const int cy = static_cast<int>(std::round(center.y()));

    QImage patch(patchW, patchH, QImage::Format_ARGB32);
    patch.fill(Qt::black);

    for (int py = 0; py < patchH; ++py) {
        for (int px = 0; px < patchW; ++px) {
            const int sx = cx - half + px;
            const int sy = cy - half + py;
            if (sx >= 0 && sx < image.width() && sy >= 0 && sy < image.height())
                patch.setPixel(px, py, image.pixel(sx, sy));
        }
    }
    return patch;
}

void Tracker::setReferenceFrame(const QImage& frame, const CornerSet& corners)
{
    m_refFrame = frame.convertToFormat(QImage::Format_ARGB32);
    m_lastCorners = corners;

    const QPointF cornerPts[4] = {corners.tl, corners.tr, corners.br, corners.bl};
    for (int i = 0; i < 4; ++i)
        m_patches[i] = extractPatch(m_refFrame, cornerPts[i], m_params.patchSizePx);

    m_initialized = true;
}

QPointF Tracker::refinePoint(const QImage& source, const QPointF& current,
                              const QImage& templatePatch) const
{
    const int radius = static_cast<int>(m_params.searchRadiusPx);
    const int patchW = templatePatch.width();
    const int patchH = templatePatch.height();
    const int halfW = patchW / 2;
    const int halfH = patchH / 2;

    double bestSAD = std::numeric_limits<double>::max();
    int bestDx = 0;
    int bestDy = 0;

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int cx = static_cast<int>(std::round(current.x())) + dx;
            const int cy = static_cast<int>(std::round(current.y())) + dy;

            double sad = 0.0;
            for (int py = 0; py < patchH; ++py) {
                for (int px = 0; px < patchW; ++px) {
                    const int sx = cx - halfW + px;
                    const int sy = cy - halfH + py;
                    QRgb srcPx = 0;
                    if (sx >= 0 && sx < source.width() && sy >= 0 && sy < source.height())
                        srcPx = source.pixel(sx, sy);
                    const QRgb tmplPx = templatePatch.pixel(px, py);
                    sad += std::abs(qGray(srcPx) - qGray(tmplPx));
                }
            }
            if (sad < bestSAD) {
                bestSAD = sad;
                bestDx = dx;
                bestDy = dy;
            }
        }
    }
    return QPointF(current.x() + bestDx, current.y() + bestDy);
}

Frame Tracker::trackNextFrame(const QImage& nextFrame, int frameIndex, qint64 timeMs)
{
    Frame result;
    result.frameIndex = frameIndex;
    result.timeMs = timeMs;
    result.corners = m_lastCorners;
    result.confidence = 0.0;

    if (!m_initialized)
        return result;

    const QImage frame = nextFrame.convertToFormat(QImage::Format_ARGB32);

    const QPointF* oldPts[4] = {
        &m_lastCorners.tl, &m_lastCorners.tr, &m_lastCorners.br, &m_lastCorners.bl
    };

    QPointF newPts[4];
    double totalSAD = 0.0;
    const int patchPixels = static_cast<int>(m_params.patchSizePx * m_params.patchSizePx);

    for (int i = 0; i < 4; ++i) {
        const QPointF candidate = refinePoint(frame, *oldPts[i], m_patches[i]);

        const double alpha = m_params.dampingFactor;
        newPts[i] = QPointF(
            oldPts[i]->x() + alpha * (candidate.x() - oldPts[i]->x()),
            oldPts[i]->y() + alpha * (candidate.y() - oldPts[i]->y())
        );

        const int halfW = static_cast<int>(m_params.patchSizePx) / 2;
        const int halfH = static_cast<int>(m_params.patchSizePx) / 2;
        const int patchW = m_patches[i].width();
        const int patchH = m_patches[i].height();
        double sad = 0.0;
        for (int py = 0; py < patchH; ++py) {
            for (int px = 0; px < patchW; ++px) {
                const int sx = static_cast<int>(std::round(newPts[i].x())) - halfW + px;
                const int sy = static_cast<int>(std::round(newPts[i].y())) - halfH + py;
                QRgb srcPx = 0;
                if (sx >= 0 && sx < frame.width() && sy >= 0 && sy < frame.height())
                    srcPx = frame.pixel(sx, sy);
                const QRgb tmplPx = m_patches[i].pixel(px, py);
                sad += std::abs(qGray(srcPx) - qGray(tmplPx));
            }
        }
        totalSAD += sad;
    }

    CornerSet newCorners;
    newCorners.tl = newPts[0];
    newCorners.tr = newPts[1];
    newCorners.br = newPts[2];
    newCorners.bl = newPts[3];

    const double validFactor = newCorners.isValid() ? 1.0 : 0.5;
    const double avgSADPerPixel = (patchPixels > 0)
        ? (totalSAD / (4.0 * static_cast<double>(patchPixels) * 255.0))
        : 1.0;
    const double conf = std::max(0.0, std::min(1.0, (1.0 - avgSADPerPixel) * validFactor));

    if (newCorners.isValid()) {
        m_lastCorners = newCorners;
        result.corners = newCorners;
    } else {
        result.corners = m_lastCorners;
    }
    result.confidence = conf;
    return result;
}

QList<Frame> Tracker::trackSequence(const QList<QImage>& frames,
                                     const CornerSet& initialCorners,
                                     qint64 frameDurationMs)
{
    QList<Frame> results;

    if (frames.isEmpty() || !initialCorners.isValid())
        return results;

    const int limit = (m_params.maxFramesPerCall > 0)
        ? std::min(m_params.maxFramesPerCall, static_cast<int>(frames.size()))
        : static_cast<int>(frames.size());

    setReferenceFrame(frames[0], initialCorners);

    Frame refFrame;
    refFrame.frameIndex = 0;
    refFrame.timeMs = 0;
    refFrame.corners = initialCorners;
    refFrame.confidence = 1.0;
    results.append(refFrame);

    for (int i = 1; i < limit; ++i) {
        results.append(trackNextFrame(frames[i], i, static_cast<qint64>(i) * frameDurationMs));
    }

    return results;
}

// ---------------------------------------------------------------------------
// Debug helper
// ---------------------------------------------------------------------------

QString homographyToString(const Homography& h)
{
    return QString("[%1 %2 %3 | %4 %5 %6 | %7 %8 %9]")
        .arg(h.m[0], 0, 'f', 4).arg(h.m[1], 0, 'f', 4).arg(h.m[2], 0, 'f', 4)
        .arg(h.m[3], 0, 'f', 4).arg(h.m[4], 0, 'f', 4).arg(h.m[5], 0, 'f', 4)
        .arg(h.m[6], 0, 'f', 4).arg(h.m[7], 0, 'f', 4).arg(h.m[8], 0, 'f', 4);
}

} // namespace planar
