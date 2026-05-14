#pragma once

#include "ObsScanner.h"
#include "ObsProfile.h"

#include <QString>
#include <QList>

namespace obs::layout {

struct TimelineClipPlacement {
    int     trackIndex  = 0;
    qint64  startMs     = 0;
    qint64  durationMs  = 0;
    QString filePath;
    QString displayName;  // QFileInfo::baseName() of filePath
};

// Lay out a list of RecordingGroups onto timeline tracks.
// Track assignment:
//   0        — primary video
//   1..N     — audio track files (capped at track 31)
//   3+       — source record files (sourceRecordTrackIndexBase = 3, parallel)
//   last     — replay buffer files (appended after all groups)
// startMs is computed as an accumulator across groups (sorted by startedAt).
// Returns placements sorted by startMs ascending.
// Never throws.
QList<TimelineClipPlacement> layoutToTimeline(
    const QList<obs::scan::RecordingGroup>  &groups,
    const QList<obs::profile::SceneInfo>    &scenes);

} // namespace obs::layout
