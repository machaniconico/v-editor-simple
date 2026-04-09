#pragma once

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector>

class QPainterPath;

// --- Cubic bezier control point with in/out handles ---

struct RotoPoint {
    QPointF position;
    QPointF handleIn;   // tangent handle towards previous point
    QPointF handleOut;  // tangent handle towards next point

    QJsonObject toJson() const;
    static RotoPoint fromJson(const QJsonObject &obj);
};

// --- Bezier spline path built from RotoPoints ---

struct RotoPath {
    QVector<RotoPoint> points;
    bool closed = true;
    double feather = 0.0;  // feather radius in pixels

    QJsonObject toJson() const;
    static RotoPath fromJson(const QJsonObject &obj);
};

// --- Keyframe holding a RotoPath at a specific frame ---

struct RotoKeyframe {
    int frameNumber = 0;
    RotoPath path;

    QJsonObject toJson() const;
    static RotoKeyframe fromJson(const QJsonObject &obj);
};

// --- Rotoscope ---

class Rotoscope
{
public:
    Rotoscope() = default;

    // --- Keyframe management ---

    void addKeyframe(int frameNumber, const RotoPath &path);
    void removeKeyframe(int frameNumber);

    // All keyframes sorted by frame number
    const QVector<RotoKeyframe> &keyframes() const { return m_keyframes; }

    // --- Path interpolation ---

    // Interpolate path between surrounding keyframes at the given frame
    RotoPath getPathAtFrame(int frameNumber) const;

    // Lerp between two RotoPath shapes point-by-point (t in 0..1)
    // If point counts differ, the shorter path is padded by duplicating its last point
    static RotoPath interpolatePaths(const RotoPath &pathA, const RotoPath &pathB, double t);

    // --- Mask rendering ---

    // Generate a grayscale mask QImage from the interpolated path at the given frame
    QImage renderMask(int frameNumber, const QSize &canvasSize) const;

    // Generate masks for a contiguous range of frames
    QVector<QImage> renderMaskSequence(int startFrame, int endFrame,
                                       const QSize &canvasSize) const;

    // Apply roto mask (grayscale) to a video frame (alpha multiply)
    static QImage applyToFrame(const QImage &sourceFrame, const QImage &maskFrame);

    // --- Conversion helpers ---

    // Convert a RotoPath to a QPainterPath for rendering
    static QPainterPath pathToQPainterPath(const RotoPath &rotoPath);

    // --- Feathering ---

    // Apply Gaussian-like feathering to a binary mask (iterative box blur)
    static QImage estimateFeather(const RotoPath &path, const QSize &canvasSize);

    // --- Export ---

    // Save mask images as PNG files (one per frame)
    bool exportMaskSequence(int startFrame, int endFrame,
                            const QSize &canvasSize, const QString &outputDir) const;

    // --- Serialisation ---

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    // Find the two surrounding keyframes for a given frame number
    // Returns false if no keyframes exist
    bool findBracketingKeyframes(int frameNumber,
                                 const RotoKeyframe *&prev,
                                 const RotoKeyframe *&next) const;

    // Iterative box blur (fast Gaussian approximation), single channel
    static void boxBlur(QImage &img, int radius);

    QVector<RotoKeyframe> m_keyframes;  // sorted by frameNumber
};
