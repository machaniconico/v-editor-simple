#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include "WaveformGenerator.h"

// Detected silent region
struct SilenceRegion {
    double startTime;  // seconds
    double endTime;
    double duration() const { return endTime - startTime; }
};

// Detected scene change
struct SceneChange {
    double time;       // seconds
    double confidence; // 0.0-1.0
};

// Auto-edit settings
struct AutoEditConfig {
    // Silence detection
    double silenceThreshold = 0.02;    // amplitude threshold (0-1)
    double minSilenceDuration = 0.5;   // minimum silence length (seconds)
    double paddingBefore = 0.1;        // keep this much audio before speech
    double paddingAfter = 0.05;        // keep this much audio after speech

    // Scene detection
    double sceneChangeThreshold = 0.3; // pixel difference threshold (0-1)
    int sceneCheckInterval = 5;        // check every N frames
};

class AutoEdit : public QObject
{
    Q_OBJECT

public:
    explicit AutoEdit(QObject *parent = nullptr);

    // Detect silence regions from waveform data
    static QVector<SilenceRegion> detectSilence(const WaveformData &waveform,
                                                 const AutoEditConfig &config = {});

    // Detect silence from audio file (generates waveform internally)
    static QVector<SilenceRegion> detectSilenceFromFile(const QString &filePath,
                                                         const AutoEditConfig &config = {});

    // Generate cut points: returns timestamps where the clip should be split
    // Removes silent regions, keeping padding for natural flow
    static QVector<double> generateJumpCuts(const QVector<SilenceRegion> &silences,
                                             double totalDuration,
                                             const AutoEditConfig &config = {});

    // Detect scene changes from video file
    static QVector<SceneChange> detectSceneChanges(const QString &filePath,
                                                    const AutoEditConfig &config = {});

signals:
    void progressChanged(int percent);
    void analysisComplete(const QVector<SilenceRegion> &silences,
                          const QVector<SceneChange> &scenes);
};
