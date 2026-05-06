#pragma once

#include <optional>
#include <vector>
#include <QElapsedTimer>

class Timeline;
class MarkerManager;

enum class SnapTargetKind {
    ClipStart,
    ClipEnd,
    Playhead,
    Origin,
    Marker
};

struct SnapTarget {
    qint64 timeUs = 0;
    SnapTargetKind kind = SnapTargetKind::ClipStart;
    int sourceTrackIndex = -1;
    int sourceClipIndex = -1;
};

class SnapEngine
{
public:
    void collectFromTimeline(const Timeline &timeline, qint64 currentPlayheadUs,
                             const MarkerManager *markers);

    std::optional<SnapTarget> findMatch(qint64 candidateUs, qint64 toleranceUs) const;

    bool wasJustEngaged() const;
    qint64 lastMatchTimeUs() const;

private:
    std::vector<SnapTarget> m_targets;
    mutable qint64 m_lastMatchTimeUs = 0;
    mutable QElapsedTimer m_lastMatchTimer;
    static constexpr qint64 kEngagementWindowMs = 200;
};
