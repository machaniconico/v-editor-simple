#include "SnapEngine.h"
#include "Timeline.h"
#include "TimelineMarker.h"
#include <algorithm>
#include <cmath>

void SnapEngine::collectFromTimeline(const Timeline &timeline, qint64 currentPlayheadUs,
                                     const MarkerManager *markers)
{
    m_targets.clear();

    auto collectTrack = [&](const QVector<TimelineTrack *> &tracks, bool isVideo) {
        for (int ti = 0; ti < tracks.size(); ++ti) {
            const auto *track = tracks[ti];
            if (!track) continue;
            double cursorSec = 0.0;
            for (int ci = 0; ci < track->clipCount(); ++ci) {
                const auto &clip = track->clips()[ci];
                cursorSec += clip.leadInSec;
                const qint64 startUs = static_cast<qint64>(cursorSec * 1'000'000.0);
                cursorSec += clip.effectiveDuration();
                const qint64 endUs = static_cast<qint64>(cursorSec * 1'000'000.0);
                const int trackIndex = isVideo ? ti : (ti + 1000);
                m_targets.push_back({startUs, SnapTargetKind::ClipStart, trackIndex, ci});
                m_targets.push_back({endUs,   SnapTargetKind::ClipEnd,   trackIndex, ci});
            }
        }
    };

    collectTrack(timeline.videoTracks(), true);
    collectTrack(timeline.audioTracks(), false);

    m_targets.push_back({currentPlayheadUs, SnapTargetKind::Playhead, -1, -1});
    m_targets.push_back({0, SnapTargetKind::Origin, -1, -1});

    if (markers && !markers->isEmpty()) {
        for (const auto &m : markers->markers()) {
            m_targets.push_back({static_cast<qint64>(m.time * 1'000'000.0),
                                 SnapTargetKind::Marker, -1, -1});
        }
    }

    std::sort(m_targets.begin(), m_targets.end(),
              [](const SnapTarget &a, const SnapTarget &b) {
                  return a.timeUs < b.timeUs;
              });
}

std::optional<SnapTarget> SnapEngine::findMatch(qint64 candidateUs, qint64 toleranceUs) const
{
    if (m_targets.empty())
        return std::nullopt;

    auto it = std::lower_bound(m_targets.begin(), m_targets.end(), candidateUs,
                               [](const SnapTarget &t, qint64 val) { return t.timeUs < val; });

    const SnapTarget *best = nullptr;
    qint64 bestDist = toleranceUs + 1;

    auto consider = [&](const SnapTarget &t) {
        const qint64 d = std::abs(t.timeUs - candidateUs);
        if (d <= toleranceUs && d < bestDist) {
            best = &t;
            bestDist = d;
        }
    };

    if (it != m_targets.end())
        consider(*it);
    if (it != m_targets.begin())
        consider(*(it - 1));

    if (best) {
        m_lastMatchTimeUs = best->timeUs;
        m_lastMatchTimer.restart();
        return *best;
    }
    return std::nullopt;
}

bool SnapEngine::wasJustEngaged() const
{
    return m_lastMatchTimer.isValid()
        && m_lastMatchTimer.elapsed() < kEngagementWindowMs;
}

qint64 SnapEngine::lastMatchTimeUs() const
{
    return m_lastMatchTimeUs;
}
