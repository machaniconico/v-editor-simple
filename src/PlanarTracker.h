#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <QString>

class QJsonObject;

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
