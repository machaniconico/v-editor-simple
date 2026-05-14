#pragma once

#include <QString>
#include <QList>
#include <QDateTime>

namespace obs::scan {

struct RecordingGroup {
    QString      sessionId;          // stem of the primary video file
    QString      primaryVideoFile;   // absolute path
    QList<QString> audioTrackFiles;  // -track1.aac / -track2.aac etc.
    QList<QString> replayBufferFiles;
    QList<QString> sourceRecordFiles;
    QList<QString> sceneSegmentFiles;
    QDateTime    startedAt;
    double       durationSec = 0.0;
};

// Recursively scan folderPath for OBS recording files.
// Returns one RecordingGroup per detected session.
// Returns an empty list if no matching files are found.
// Never throws.
QList<RecordingGroup> scanFolder(const QString &folderPath);

} // namespace obs::scan
