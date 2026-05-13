#pragma once

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QList>
#include <QString>
#include <QRectF>

#include <array>
#include <cstdint>
#include <vector>

class QJsonObject;

// ---------------------------------------------------------------------------
// namespace planartrack  — original Lucas-Kanade tracker (used by ProjectFile,
//                          VideoStabilizer, etc.)
// ---------------------------------------------------------------------------

namespace planartrack {

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

struct Quad {
    Point2D tl;
    Point2D tr;
    Point2D br;
    Point2D bl;
};

struct LumaImage {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    int stride = 0;

    LumaImage() = default;
    LumaImage(int w, int h)
        : pixels(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 0)
        , width(w)
        , height(h)
        , stride(w) {}
};

using Homography = std::array<double, 9>;

struct FrameResult {
    int frameIndex = 0;
    Homography H = {1.0, 0.0, 0.0,
                    0.0, 1.0, 0.0,
                    0.0, 0.0, 1.0};
    double residual = 255.0;
    double confidence = 0.0;
    bool uncertain = true;
};

struct SourceClipKey {
    QString filePath;
    int sourceTrack = 0;
    int sourceClipIndex = 0;
};

struct PlanarTrack {
    QString trackId;
    QString name;
    SourceClipKey sourceClipKey;
    int refFrameIndex = 0;
    Quad refQuad;
    std::vector<FrameResult> frames;
};

QJsonObject toJson(const PlanarTrack& track);
bool fromJson(const QJsonObject& obj, PlanarTrack& track);

bool isIdentityHomography(const Homography& H, double tolerance = 1e-12);
bool shouldSkipOnSave(const PlanarTrack& track);

class PlanarTracker {
public:
    PlanarTracker(const LumaImage& ref, const Quad& refQuad);
    FrameResult track(const LumaImage& frame, int frameIndex);

    struct Sample {
        double x = 0.0;
        double y = 0.0;
        double reference = 0.0;
        double referenceZeroMean = 0.0;
        double gradX = 0.0;
        double gradY = 0.0;
    };

    struct TemplateLevel {
        Quad quad;
        int scaleDivisor = 1;
        double referenceMean = 0.0;
        std::vector<Sample> samples;
    };

private:
    std::vector<LumaImage> m_referencePyramid;
    std::vector<TemplateLevel> m_templates;
    Homography m_lastH = {1.0, 0.0, 0.0,
                          0.0, 1.0, 0.0,
                          0.0, 0.0, 1.0};
    bool m_hasLastH = false;
};

#ifdef VEDITOR_PLANAR_TRACKER_SELFTEST
namespace tests {
bool runSelfTest();
}
#endif

} // namespace planartrack

// ---------------------------------------------------------------------------
// namespace planar  — SAD-based 4-corner planar tracker (Sprint 15, A1)
// ---------------------------------------------------------------------------

namespace planar {

// 4 corner positions (image coordinate, pixels; order: tl, tr, br, bl)
struct CornerSet {
    QPointF tl = QPointF(0, 0);
    QPointF tr = QPointF(0, 0);
    QPointF br = QPointF(0, 0);
    QPointF bl = QPointF(0, 0);

    bool isValid() const;
    QPointF center() const;
    static CornerSet rectangle(const QRectF& rect);
};

// Per-frame track result
struct Frame {
    int frameIndex = 0;
    qint64 timeMs = 0;
    CornerSet corners;
    double confidence = 1.0;
};

struct TrackParams {
    double searchRadiusPx = 16.0;
    double patchSizePx = 32.0;
    double dampingFactor = 0.3;
    int maxFramesPerCall = 0;
};

// 3x3 homography (row-major)
struct Homography {
    double m[9] = {1,0,0, 0,1,0, 0,0,1};
};

Homography homographyFromCorners(const CornerSet& src, const CornerSet& dst);
QPointF transformPoint(const QPointF& pt, const Homography& h);
QImage warpImage(const QImage& source, const Homography& h, const QSize& outSize);

class Tracker {
public:
    Tracker();

    void setParams(const TrackParams& p);
    TrackParams params() const;

    void setReferenceFrame(const QImage& frame, const CornerSet& corners);
    Frame trackNextFrame(const QImage& nextFrame, int frameIndex, qint64 timeMs);
    QList<Frame> trackSequence(const QList<QImage>& frames,
                                const CornerSet& initialCorners,
                                qint64 frameDurationMs = 33);

    void reset();
    bool isInitialized() const { return m_initialized; }
    CornerSet lastCorners() const { return m_lastCorners; }

private:
    QPointF refinePoint(const QImage& source, const QPointF& current,
                        const QImage& templatePatch) const;
    QImage extractPatch(const QImage& image, const QPointF& center, double size) const;

    TrackParams m_params;
    bool m_initialized = false;
    QImage m_refFrame;
    CornerSet m_lastCorners;
    QImage m_patches[4];
};

QString homographyToString(const Homography& h);

} // namespace planar
