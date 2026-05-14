#include "ObsLayout.h"

#include <QFileInfo>
#include <QDebug>
#include <algorithm>

namespace obs::layout {

static constexpr int kMaxTrackIndex          = 31;
static constexpr int kSourceRecordTrackBase  = 3;

QList<TimelineClipPlacement> layoutToTimeline(
    const QList<obs::scan::RecordingGroup>  &groups,
    const QList<obs::profile::SceneInfo>    &scenes)
{
    Q_UNUSED(scenes) // reserved for future scene-aware routing

    QList<TimelineClipPlacement> result;

    // Sort groups chronologically so startMs accumulator is monotone
    QList<obs::scan::RecordingGroup> sorted = groups;
    std::sort(sorted.begin(), sorted.end(),
              [](const obs::scan::RecordingGroup &a,
                 const obs::scan::RecordingGroup &b) {
                  return a.startedAt < b.startedAt;
              });

    // Replay buffers are collected across all groups and appended at the end
    QList<TimelineClipPlacement> replayPlacements;

    qint64 startMs = 0;

    for (const obs::scan::RecordingGroup &grp : sorted) {
        const qint64 durationMs = static_cast<qint64>(grp.durationSec * 1000.0);

        // --- Track 0: primary video ---
        if (!grp.primaryVideoFile.isEmpty()) {
            TimelineClipPlacement p;
            p.trackIndex  = 0;
            p.startMs     = startMs;
            p.durationMs  = durationMs;
            p.filePath    = grp.primaryVideoFile;
            p.displayName = QFileInfo(grp.primaryVideoFile).baseName();
            result.append(p);
        }

        // --- Tracks 1..N: audio track files ---
        int audioTrack = 1;
        for (const QString &audioFile : grp.audioTrackFiles) {
            TimelineClipPlacement p;
            p.trackIndex  = qMin(audioTrack, kMaxTrackIndex);
            p.startMs     = startMs;
            p.durationMs  = durationMs;
            p.filePath    = audioFile;
            p.displayName = QFileInfo(audioFile).baseName();
            result.append(p);
            if (audioTrack < kMaxTrackIndex)
                ++audioTrack;
        }

        // --- Tracks sourceRecordTrackBase+: source record files (parallel) ---
        int srcTrack = kSourceRecordTrackBase;
        for (const QString &srcFile : grp.sourceRecordFiles) {
            TimelineClipPlacement p;
            p.trackIndex  = qMin(srcTrack, kMaxTrackIndex);
            p.startMs     = startMs;
            p.durationMs  = durationMs;
            p.filePath    = srcFile;
            p.displayName = QFileInfo(srcFile).baseName();
            result.append(p);
            if (srcTrack < kMaxTrackIndex)
                ++srcTrack;
        }

        // --- Replay buffers: collect for later ---
        for (const QString &rbFile : grp.replayBufferFiles) {
            TimelineClipPlacement p;
            p.trackIndex  = 0; // will be placed after accumulation
            p.startMs     = startMs; // tentative; will sort correctly
            p.durationMs  = durationMs;
            p.filePath    = rbFile;
            p.displayName = QFileInfo(rbFile).baseName();
            replayPlacements.append(p);
        }

        startMs += (durationMs > 0 ? durationMs : 0);
    }

    // Append replay buffers after all regular clips
    result.append(replayPlacements);

    // Sort by startMs ascending (stable to preserve intra-group order)
    std::stable_sort(result.begin(), result.end(),
                     [](const TimelineClipPlacement &a,
                        const TimelineClipPlacement &b) {
                         return a.startMs < b.startMs;
                     });

    return result;
}

} // namespace obs::layout
