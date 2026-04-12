#pragma once

#include <QString>
#include <QMetaType>
#include <QVector>

// Resolved playback descriptor used to communicate the timeline schedule from
// Timeline to VideoPlayer. Independent from ClipInfo (which carries editor-side
// metadata like waveforms, effects, keyframes) so VideoPlayer stays lean and
// Timeline.h's heavy includes don't leak into the player.
struct PlaybackEntry {
    QString filePath;            // Path to the underlying media file
    double clipIn = 0.0;         // File-local start (in_point), seconds
    double clipOut = 0.0;        // File-local end (out_point or full duration), seconds
    double timelineStart = 0.0;  // Where this entry begins on the timeline, seconds
    double timelineEnd = 0.0;    // Where this entry ends on the timeline (exclusive), seconds
    double speed = 1.0;          // Playback speed multiplier (>0)
    int sourceTrack = 0;         // 0 = V1, 1 = V2, ... (higher = front in stacking)
    bool audioMuted = false;     // Audio for this entry is muted (corresponding A track muted)
};

Q_DECLARE_METATYPE(PlaybackEntry)
Q_DECLARE_METATYPE(QVector<PlaybackEntry>)
