#pragma once

#include <QJsonObject>
#include <QPointF>

// ---------------------------------------------------------------------------
// WiggleTransform — time-varying 2D transform jitter (namespace wiggle)
//
// Produces smooth, deterministic, fbm-based position/rotation/scale offsets
// suitable for "handheld camera" or "jitter" simulation on a clip.
// No third-party dependencies — QtCore/QtGui + std only.
// CMake wiring deferred to a later story.
// ---------------------------------------------------------------------------

namespace wiggle {

// ---------------------------------------------------------------------------
// WiggleParams — all parameters for one wiggle instance
// ---------------------------------------------------------------------------
struct WiggleParams {
    double   positionFrequency    = 2.0;            // Hz
    QPointF  positionAmplitude    = QPointF(0, 0);  // pixels, signed
    double   rotationFrequency    = 2.0;            // Hz
    double   rotationAmplitudeDeg = 0.0;            // degrees
    double   scaleFrequency       = 2.0;            // Hz
    double   scaleAmplitude       = 0.0;            // fraction (0.05 = ±5 %)
    unsigned int seed             = 1;
    int      octaves              = 2;              // fbm octaves, clamped [1,6]
    bool     enabled              = false;
};

// ---------------------------------------------------------------------------
// WiggleOffset — output of evaluate()
// ---------------------------------------------------------------------------
struct WiggleOffset {
    QPointF positionOffset    = QPointF(0, 0); // pixels to translate
    double  rotationOffsetDeg = 0.0;           // degrees to rotate
    double  scaleMultiplier   = 1.0;           // multiplicative scale factor
};

// ---------------------------------------------------------------------------
// evaluate
//
// Returns the wiggle offset for the given time in seconds.
// If !p.enabled, returns the identity offset {{0,0}, 0.0, 1.0}.
// Deterministic for a fixed (seed, time) pair; different seeds yield
// independent trajectories.  O(octaves) per call.  No NaN/inf.
// ---------------------------------------------------------------------------
WiggleOffset evaluate(const WiggleParams& p, double timeSeconds);

// ---------------------------------------------------------------------------
// Presets — return enabled WiggleParams ready to use
// intensity = 1.0 → nominal values; scale linearly with intensity
// ---------------------------------------------------------------------------

// Moderate position amp (~6*intensity px), low freq (~1.5 Hz), 2 octaves
WiggleParams handheldPreset(double intensity = 1.0);

// Higher freq (~5 Hz) and amplitude (~10*intensity px, ~1.5*intensity deg), 3 octaves
WiggleParams nervousPreset(double intensity = 1.0);

// Very low freq (~0.6 Hz), small amp (~3*intensity px), tiny rotation, 2 octaves
WiggleParams floatPreset(double intensity = 1.0);

// ---------------------------------------------------------------------------
// Serialisation — round-trips all WiggleParams fields
// QPointF serialised as {"x":..,"y":..}
// ---------------------------------------------------------------------------
QJsonObject  toJson(const WiggleParams& p);
WiggleParams fromJson(const QJsonObject& o);

} // namespace wiggle
