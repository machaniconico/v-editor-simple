#pragma once
#include <QVector>

// Sidechain ducking parameters
struct DuckingParams {
    double thresholdDb     = -30.0; // sidechain level above which reduction activates (dBFS)
    double targetReductionDb = -12.0; // amount to reduce program signal (negative dB)
    double attackMs        =  20.0; // time constant for gain reduction onset (ms)
    double releaseMs       = 300.0; // time constant for gain recovery (ms)
    double holdMs          = 200.0; // hold time before release begins after sidechain drops (ms)
    double kneeDb          =   6.0; // reserved for future soft-knee; not used in current algorithm
};

// Compute per-sample gain envelope from a sidechain signal.
// Output size == sidechain.size(). Values clamped to [minGainLinear(p), 1.0].
QVector<float> computeDuckingGain(const QVector<float>& sidechain,
                                  int sampleRate,
                                  const DuckingParams& p);

// Multiply program signal element-wise by the gain envelope.
// Output size == min(program.size(), duckGain.size()).
// Safe when sizes differ; excess program samples are dropped.
QVector<float> applyDucking(const QVector<float>& program,
                             const QVector<float>& duckGain);

// Linear amplitude corresponding to targetReductionDb (e.g. -12 dB -> ~0.2512)
double minGainLinear(const DuckingParams& p);
