#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include "ProjectSettings.h"
#include "Timeline.h"
#include "VideoEffect.h"
#include "Keyframe.h"

// Full project state for serialization
struct ProjectData {
    ProjectConfig config;
    QVector<QVector<ClipInfo>> videoTracks;
    QVector<QVector<ClipInfo>> audioTracks;
    double playheadPos = 0.0;
    double markIn = -1.0;
    double markOut = -1.0;
    int zoomLevel = 10;
};

class ProjectFile
{
public:
    static bool save(const QString &filePath, const ProjectData &data);
    static bool load(const QString &filePath, ProjectData &data);

    static const QString fileFilter() { return "V Editor Project (*.veditor);;All Files (*)"; }

private:
    // Serialization helpers
    static QJsonObject configToJson(const ProjectConfig &config);
    static ProjectConfig configFromJson(const QJsonObject &obj);

    static QJsonObject clipToJson(const ClipInfo &clip);
    static ClipInfo clipFromJson(const QJsonObject &obj);

    static QJsonObject colorCorrectionToJson(const ColorCorrection &cc);
    static ColorCorrection colorCorrectionFromJson(const QJsonObject &obj);

    static QJsonObject effectToJson(const VideoEffect &effect);
    static VideoEffect effectFromJson(const QJsonObject &obj);

    static QJsonObject keyframeTrackToJson(const KeyframeTrack &track);
    static KeyframeTrack keyframeTrackFromJson(const QJsonObject &obj);

    static QJsonObject keyframeManagerToJson(const KeyframeManager &km);
    static KeyframeManager keyframeManagerFromJson(const QJsonObject &obj);

    static QJsonArray tracksToJson(const QVector<QVector<ClipInfo>> &tracks);
    static QVector<QVector<ClipInfo>> tracksFromJson(const QJsonArray &arr);
};
