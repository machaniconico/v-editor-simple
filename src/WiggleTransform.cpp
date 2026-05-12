#include "WiggleTransform.h"

#include <QJsonValue>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Internal helpers — value noise + fbm
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// hash32 — fast deterministic hash of two unsigned ints → unsigned int
//
// Uses a variant of the "triple32" finaliser so that consecutive integer
// inputs produce visually uncorrelated outputs.
// ---------------------------------------------------------------------------
static unsigned int hash32(unsigned int a, unsigned int b)
{
    // Combine inputs
    unsigned int h = a ^ (b * 2654435761u);
    // Finalise
    h ^= h >> 16;
    h *= 0x45d9f3bu;
    h ^= h >> 16;
    return h;
}

// ---------------------------------------------------------------------------
// hashToDouble — map an unsigned int to [-1, 1]
// ---------------------------------------------------------------------------
static double hashToDouble(unsigned int h)
{
    // Map to [0, 1] then to [-1, 1]
    return (static_cast<double>(h) / static_cast<double>(0xFFFFFFFFu)) * 2.0 - 1.0;
}

// ---------------------------------------------------------------------------
// smoothstep — cubic Hermite interpolation in [0, 1]
// ---------------------------------------------------------------------------
static double smoothstep(double t)
{
    return t * t * (3.0 - 2.0 * t);
}

// ---------------------------------------------------------------------------
// valueNoise1D — deterministic lattice value noise on the real line
//
// Hashes integer lattice points seeded with `seed`, then interpolates
// smoothly between them.  Returns a value in roughly [-1, 1].
// ---------------------------------------------------------------------------
static double valueNoise1D(unsigned int seed, double x)
{
    // Integer lattice cell
    int    ix  = static_cast<int>(std::floor(x));
    double fx  = x - std::floor(x); // fractional in [0, 1)

    // Hash lattice corners
    double v0 = hashToDouble(hash32(seed, static_cast<unsigned int>(ix)));
    double v1 = hashToDouble(hash32(seed, static_cast<unsigned int>(ix + 1)));

    // Smooth interpolation
    double t = smoothstep(fx);
    return v0 + t * (v1 - v0);
}

// ---------------------------------------------------------------------------
// fbm1D — fractional Brownian motion (sum of octaves of value noise)
//
// Doubles frequency and halves amplitude per octave.  Sum is roughly in
// [-1, 1] (actual range tightens with more octaves due to cancellation).
// `octaves` is clamped to [1, 6] here so the caller doesn't have to.
// ---------------------------------------------------------------------------
static double fbm1D(unsigned int seed, double x, int octaves)
{
    octaves = std::max(1, std::min(6, octaves));

    double result    = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    double norm      = 0.0; // sum of amplitudes for normalisation

    for (int i = 0; i < octaves; ++i) {
        result    += amplitude * valueNoise1D(seed + static_cast<unsigned int>(i * 31337), x * frequency);
        norm      += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    // Normalise so the output remains in [-1, 1]
    return (norm > 0.0) ? (result / norm) : 0.0;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// wiggle API
// ---------------------------------------------------------------------------

namespace wiggle {

// ---------------------------------------------------------------------------
// evaluate
// ---------------------------------------------------------------------------
WiggleOffset evaluate(const WiggleParams& p, double timeSeconds)
{
    if (!p.enabled) {
        return WiggleOffset{QPointF(0, 0), 0.0, 1.0};
    }

    // Four independent seed channels — one per axis
    // Use complementary bit-mix constants so channels are uncorrelated even
    // for small seed values such as 0 or 1.
    const unsigned int seedX = p.seed;
    const unsigned int seedY = p.seed ^ 0x9E3779B9u;
    const unsigned int seedR = p.seed * 2654435761u;
    const unsigned int seedS = (p.seed ^ 0x9E3779B9u) * 2654435761u;

    const int oct = std::max(1, std::min(6, p.octaves));

    const double tPos = timeSeconds * p.positionFrequency;
    const double tRot = timeSeconds * p.rotationFrequency;
    const double tScl = timeSeconds * p.scaleFrequency;

    WiggleOffset out;
    out.positionOffset.setX(fbm1D(seedX, tPos, oct) * p.positionAmplitude.x());
    out.positionOffset.setY(fbm1D(seedY, tPos, oct) * p.positionAmplitude.y());
    out.rotationOffsetDeg = fbm1D(seedR, tRot, oct) * p.rotationAmplitudeDeg;
    out.scaleMultiplier   = 1.0 + fbm1D(seedS, tScl, oct) * p.scaleAmplitude;

    return out;
}

// ---------------------------------------------------------------------------
// handheldPreset
// ---------------------------------------------------------------------------
WiggleParams handheldPreset(double intensity)
{
    WiggleParams p;
    p.enabled              = true;
    p.positionFrequency    = 1.5;
    p.positionAmplitude    = QPointF(6.0 * intensity, 6.0 * intensity);
    p.rotationFrequency    = 1.5;
    p.rotationAmplitudeDeg = 0.4 * intensity;
    p.scaleFrequency       = 1.5;
    p.scaleAmplitude       = 0.005 * intensity;
    p.seed                 = 1;
    p.octaves              = 2;
    return p;
}

// ---------------------------------------------------------------------------
// nervousPreset
// ---------------------------------------------------------------------------
WiggleParams nervousPreset(double intensity)
{
    WiggleParams p;
    p.enabled              = true;
    p.positionFrequency    = 5.0;
    p.positionAmplitude    = QPointF(10.0 * intensity, 10.0 * intensity);
    p.rotationFrequency    = 5.0;
    p.rotationAmplitudeDeg = 1.5 * intensity;
    p.scaleFrequency       = 5.0;
    p.scaleAmplitude       = 0.02 * intensity;
    p.seed                 = 2;
    p.octaves              = 3;
    return p;
}

// ---------------------------------------------------------------------------
// floatPreset
// ---------------------------------------------------------------------------
WiggleParams floatPreset(double intensity)
{
    WiggleParams p;
    p.enabled              = true;
    p.positionFrequency    = 0.6;
    p.positionAmplitude    = QPointF(3.0 * intensity, 3.0 * intensity);
    p.rotationFrequency    = 0.6;
    p.rotationAmplitudeDeg = 0.15 * intensity;
    p.scaleFrequency       = 0.6;
    p.scaleAmplitude       = 0.003 * intensity;
    p.seed                 = 3;
    p.octaves              = 2;
    return p;
}

// ---------------------------------------------------------------------------
// toJson
// ---------------------------------------------------------------------------
QJsonObject toJson(const WiggleParams& p)
{
    QJsonObject o;
    o["positionFrequency"]    = p.positionFrequency;
    o["positionAmplitude"]    = QJsonObject{{"x", p.positionAmplitude.x()},
                                            {"y", p.positionAmplitude.y()}};
    o["rotationFrequency"]    = p.rotationFrequency;
    o["rotationAmplitudeDeg"] = p.rotationAmplitudeDeg;
    o["scaleFrequency"]       = p.scaleFrequency;
    o["scaleAmplitude"]       = p.scaleAmplitude;
    o["seed"]                 = static_cast<int>(p.seed);
    o["octaves"]              = p.octaves;
    o["enabled"]              = p.enabled;
    return o;
}

// ---------------------------------------------------------------------------
// fromJson
// ---------------------------------------------------------------------------
WiggleParams fromJson(const QJsonObject& o)
{
    WiggleParams p;

    if (o.contains("positionFrequency"))
        p.positionFrequency = o["positionFrequency"].toDouble(p.positionFrequency);

    if (o.contains("positionAmplitude")) {
        const QJsonObject pa = o["positionAmplitude"].toObject();
        p.positionAmplitude = QPointF(
            pa["x"].toDouble(0.0),
            pa["y"].toDouble(0.0));
    }

    if (o.contains("rotationFrequency"))
        p.rotationFrequency = o["rotationFrequency"].toDouble(p.rotationFrequency);

    if (o.contains("rotationAmplitudeDeg"))
        p.rotationAmplitudeDeg = o["rotationAmplitudeDeg"].toDouble(p.rotationAmplitudeDeg);

    if (o.contains("scaleFrequency"))
        p.scaleFrequency = o["scaleFrequency"].toDouble(p.scaleFrequency);

    if (o.contains("scaleAmplitude"))
        p.scaleAmplitude = o["scaleAmplitude"].toDouble(p.scaleAmplitude);

    if (o.contains("seed"))
        p.seed = static_cast<unsigned int>(o["seed"].toInt(static_cast<int>(p.seed)));

    if (o.contains("octaves"))
        p.octaves = o["octaves"].toInt(p.octaves);

    if (o.contains("enabled"))
        p.enabled = o["enabled"].toBool(p.enabled);

    return p;
}

} // namespace wiggle
