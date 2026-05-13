#include "AudioDucking.h"

#include <cmath>
#include <algorithm>

double minGainLinear(const DuckingParams& p)
{
    return std::pow(10.0, p.targetReductionDb / 20.0);
}

QVector<float> computeDuckingGain(const QVector<float>& sidechain,
                                  int sampleRate,
                                  const DuckingParams& p)
{
    const int n = sidechain.size();
    QVector<float> out(n, 1.0f);
    if (n == 0 || sampleRate <= 0)
        return out;

    // Clamp sample-rate-dependent time constants to safe range
    const double sr = static_cast<double>(sampleRate);

    // One-pole coefficients:  coef = 1 - exp(-1 / (timeMs * 0.001 * sr))
    // Guard against ms==0 to avoid division by zero.
    auto makeCoef = [&](double ms) -> double {
        if (ms <= 0.0) return 1.0;
        return 1.0 - std::exp(-1.0 / (ms * 0.001 * sr));
    };

    const double attackCoef  = makeCoef(p.attackMs);
    const double releaseCoef = makeCoef(p.releaseMs);

    // Hold in samples
    const int holdSamples = static_cast<int>(p.holdMs * 0.001 * sr + 0.5);

    const double minGain = minGainLinear(p);

    // Peak-follower envelope (single-pole, attack faster than release for the follower)
    // We use a simple decay-only follower: env rises instantly, decays slowly.
    // decay = 1 - exp(-1/(releaseMs*0.001*sr)) but we want a slow follower decay.
    // Use a fixed 10ms decay for the level detector so it tracks peaks closely.
    const double envDecay = 1.0 - std::exp(-1.0 / (0.010 * sr));

    double env     = 0.0; // level follower state
    double g       = 1.0; // current gain
    int    holdCnt = 0;   // remaining hold samples

    for (int i = 0; i < n; ++i) {
        // --- Level detection ---
        const double absSample = std::fabs(static_cast<double>(sidechain[i]));
        // Peak follower: instant attack, slow release
        if (absSample > env)
            env = absSample;
        else
            env += (absSample - env) * envDecay; // gentle decay toward current sample

        // Convert to dBFS (floor at -180 dBFS via 1e-9 guard)
        const double levelDb = 20.0 * std::log10(std::max(env, 1e-9));

        // --- Target gain ---
        const double target = (levelDb > p.thresholdDb) ? minGain : 1.0;

        // --- Attack / Hold / Release smoothing ---
        if (target < g) {
            // Sidechain is loud: attack (reduce gain)
            g += (target - g) * attackCoef;
            holdCnt = holdSamples; // reset hold counter
        } else {
            // Sidechain is quiet (or gain is already at target)
            if (holdCnt > 0) {
                // Hold: keep gain where it is
                holdCnt--;
            } else {
                // Release: move gain back toward 1.0
                g += (target - g) * releaseCoef;
            }
        }

        // Clamp to valid range, guard NaN/inf
        if (!(g >= minGain)) g = minGain; // catches NaN via negation
        if (g > 1.0)         g = 1.0;

        out[i] = static_cast<float>(g);
    }

    return out;
}

QVector<float> applyDucking(const QVector<float>& program,
                             const QVector<float>& duckGain)
{
    const int outSize = static_cast<int>(std::min(program.size(), duckGain.size()));
    QVector<float> out(outSize);

    for (int i = 0; i < outSize; ++i) {
        // duckGain index clamped defensively (outSize already uses min, so i < both sizes)
        out[i] = program[i] * duckGain[i];
    }

    return out;
}
