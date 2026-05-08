#pragma once

// Per-clip variable playback speed via a curve of (clipTimeUs, speed)
// keyframes. Between keyframes the speed is linearly interpolated; outside
// the last keyframe the curve holds the last keyframe's speed.
//
// Producer: Timeline (per-clip storage, JSON round-trip via ProjectFile).
// Consumers (US-INT-2 Phase A + B):
//   - VideoPlayer::entryLocalPositionUs   — timeline → source forward map
//   - VideoPlayer::fileLocalToTimelineUs  — source → timeline inverse map
//   - AudioMixer  per-fragment atempo (envvar VEDITOR_AUDIO_ATEMPO=1)
//
// The struct lives in `namespace speedramp` because there is already a
// `class SpeedRamp` in src/SpeedRamp.h that drives FFmpeg-based offline
// re-encode. The two are unrelated: this header is the in-memory curve,
// the existing class is an export pipeline.

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QtGlobal>
#include <cmath>

namespace speedramp {

struct SpeedKeyframe {
    qint64 clipTimeUs = 0;   // time within the clip (relative to clip start)
    double speed = 1.0;      // 0.1 .. 10.0 multiplier

    bool operator==(const SpeedKeyframe &other) const {
        return clipTimeUs == other.clipTimeUs
            && std::abs(speed - other.speed) < 1e-9;
    }
};

struct SpeedRamp {
    // Sorted by clipTimeUs ascending. Always at least 1 entry after any
    // mutating operation (identity / addKeyframe / removeKeyframe /
    // clearAndSetIdentity). speed is clamped to [0.1, 10.0] on insert.
    QVector<SpeedKeyframe> keyframes;

    static constexpr double kMinSpeed = 0.1;
    static constexpr double kMaxSpeed = 10.0;

    // Identity ramp: a single keyframe at clip-start with speed 1.0,
    // i.e. no time remap.
    static SpeedRamp identity() {
        SpeedRamp r;
        r.keyframes.append({0, 1.0});
        return r;
    }

    // True iff every keyframe sits at speed 1.0 (within tight epsilon).
    // Consumers can early-out the speed-curve evaluation when this is true.
    bool isIdentity() const {
        if (keyframes.isEmpty())
            return true;
        for (const auto &kf : keyframes) {
            if (std::abs(kf.speed - 1.0) > 1e-6)
                return false;
        }
        return true;
    }

    // ---- CORE API ----

    // Sample the speed at a clip-relative time using linear interpolation
    // between bracketing keyframes. Before the first keyframe → first
    // keyframe's speed. After the last → last keyframe's speed.
    double speedAt(qint64 clipTimeUs) const;

    // Given a clip-relative timeline-output time, return the corresponding
    // source-frame time. Integrates the speed curve via per-segment
    // trapezoidal area (exact for piecewise-linear speed).
    //   speed = 2.0  → source advances 2x as fast as timeline
    //   speed = 0.5  → source advances 0.5x as fast as timeline
    qint64 timelineToSourceUs(qint64 timelineRelativeUs) const;

    // Inverse of timelineToSourceUs. Walks segments forward in source
    // accumulating timeline advance until the target source time is hit.
    // Within each segment a linear-speed interpolation gives a quadratic
    // in t so the per-segment timeline-step is solved analytically (closed
    // form below); no bisection needed.
    qint64 sourceToTimelineUs(qint64 sourceRelativeUs) const;

    // Total output (timeline) duration that a source clip of length
    // sourceDuration produces under this ramp. Equivalent to
    // sourceToTimelineUs(sourceDuration) + holds-trailing-speed.
    qint64 outputDuration(qint64 sourceDuration) const;

    // ---- mutators ----

    // Insert a new keyframe (or replace if one already exists within 1 ms
    // of clipTimeUs). speed is clamped to [kMinSpeed, kMaxSpeed]. Keeps
    // the vector sorted and de-duplicated.
    void addKeyframe(qint64 clipTimeUs, double speed);

    // Remove the first keyframe within 1 ms tolerance. Always preserves at
    // least one keyframe — if removal would empty the vector, the curve
    // is reset to identity().
    void removeKeyframe(qint64 clipTimeUs);

    // Reset to the single-keyframe identity ramp.
    void clearAndSetIdentity();

    // ---- JSON ----

    QJsonObject toJson() const;
    static SpeedRamp fromJson(const QJsonObject &o);

    bool operator==(const SpeedRamp &other) const {
        return keyframes == other.keyframes;
    }
};

} // namespace speedramp
