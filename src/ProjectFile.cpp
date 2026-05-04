#include "ProjectFile.h"
#include <QFile>
#include <QJsonDocument>

static const int PROJECT_FORMAT_VERSION = 1;

bool ProjectFile::save(const QString &filePath, const ProjectData &data)
{
    QJsonObject root;
    root["version"] = PROJECT_FORMAT_VERSION;
    root["config"] = configToJson(data.config);
    root["videoTracks"] = tracksToJson(data.videoTracks);
    root["audioTracks"] = tracksToJson(data.audioTracks);
    root["playheadPos"] = data.playheadPos;
    root["markIn"] = data.markIn;
    root["markOut"] = data.markOut;
    root["zoomLevel"] = data.zoomLevel;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool ProjectFile::load(const QString &filePath, ProjectData &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) return false;

    QJsonObject root = doc.object();
    if (!root.contains("version")) return false;

    data.config = configFromJson(root["config"].toObject());
    data.videoTracks = tracksFromJson(root["videoTracks"].toArray());
    data.audioTracks = tracksFromJson(root["audioTracks"].toArray());
    data.playheadPos = root["playheadPos"].toDouble();
    data.markIn = root["markIn"].toDouble(-1.0);
    data.markOut = root["markOut"].toDouble(-1.0);
    data.zoomLevel = root["zoomLevel"].toInt(10);

    return true;
}

QString ProjectFile::toJsonString(const ProjectData &data)
{
    QJsonObject root;
    root["version"] = PROJECT_FORMAT_VERSION;
    root["config"] = configToJson(data.config);
    root["videoTracks"] = tracksToJson(data.videoTracks);
    root["audioTracks"] = tracksToJson(data.audioTracks);
    root["playheadPos"] = data.playheadPos;
    root["markIn"] = data.markIn;
    root["markOut"] = data.markOut;
    root["zoomLevel"] = data.zoomLevel;

    QJsonDocument doc(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

bool ProjectFile::fromJsonString(const QString &json, ProjectData &data)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull()) return false;

    QJsonObject root = doc.object();
    if (!root.contains("version")) return false;

    data.config = configFromJson(root["config"].toObject());
    data.videoTracks = tracksFromJson(root["videoTracks"].toArray());
    data.audioTracks = tracksFromJson(root["audioTracks"].toArray());
    data.playheadPos = root["playheadPos"].toDouble();
    data.markIn = root["markIn"].toDouble(-1.0);
    data.markOut = root["markOut"].toDouble(-1.0);
    data.zoomLevel = root["zoomLevel"].toInt(10);

    return true;
}

// --- Config ---

QJsonObject ProjectFile::configToJson(const ProjectConfig &c)
{
    QJsonObject obj;
    obj["name"] = c.name;
    obj["width"] = c.width;
    obj["height"] = c.height;
    obj["fps"] = c.fps;
    return obj;
}

ProjectConfig ProjectFile::configFromJson(const QJsonObject &obj)
{
    ProjectConfig c;
    c.name = obj["name"].toString("Untitled");
    c.width = obj["width"].toInt(1920);
    c.height = obj["height"].toInt(1080);
    c.fps = obj["fps"].toInt(30);
    return c;
}

// --- Clip ---

QJsonObject ProjectFile::clipToJson(const ClipInfo &clip)
{
    QJsonObject obj;
    obj["filePath"] = clip.filePath;
    obj["displayName"] = clip.displayName;
    obj["duration"] = clip.duration;
    obj["inPoint"] = clip.inPoint;
    obj["outPoint"] = clip.outPoint;
    obj["speed"] = clip.speed;
    obj["volume"] = clip.volume;
    obj["videoScale"] = clip.videoScale;
    obj["videoDx"] = clip.videoDx;
    obj["videoDy"] = clip.videoDy;
    obj["opacity"] = clip.opacity;

    if (!clip.colorCorrection.isDefault())
        obj["colorCorrection"] = colorCorrectionToJson(clip.colorCorrection);

    if (!clip.effects.isEmpty()) {
        QJsonArray fxArr;
        for (const auto &e : clip.effects)
            fxArr.append(effectToJson(e));
        obj["effects"] = fxArr;
    }

    if (clip.keyframes.hasAnyKeyframes())
        obj["keyframes"] = keyframeManagerToJson(clip.keyframes);

    if (clip.leadIn.type != TransitionType::None)
        obj["leadIn"] = transitionToJson(clip.leadIn);
    if (clip.trailOut.type != TransitionType::None)
        obj["trailOut"] = transitionToJson(clip.trailOut);

    return obj;
}

ClipInfo ProjectFile::clipFromJson(const QJsonObject &obj)
{
    ClipInfo clip;
    clip.filePath = obj["filePath"].toString();
    clip.displayName = obj["displayName"].toString();
    clip.duration = obj["duration"].toDouble();
    clip.inPoint = obj["inPoint"].toDouble();
    clip.outPoint = obj["outPoint"].toDouble();
    clip.speed = obj["speed"].toDouble(1.0);
    clip.volume = obj["volume"].toDouble(1.0);
    clip.videoScale = obj["videoScale"].toDouble(1.0);
    clip.videoDx = obj["videoDx"].toDouble(0.0);
    clip.videoDy = obj["videoDy"].toDouble(0.0);
    clip.opacity = obj["opacity"].toDouble(1.0);

    if (obj.contains("colorCorrection"))
        clip.colorCorrection = colorCorrectionFromJson(obj["colorCorrection"].toObject());

    if (obj.contains("effects")) {
        for (const auto &v : obj["effects"].toArray())
            clip.effects.append(effectFromJson(v.toObject()));
    }

    if (obj.contains("keyframes"))
        clip.keyframes = keyframeManagerFromJson(obj["keyframes"].toObject());

    if (obj.contains("leadIn"))
        clip.leadIn = transitionFromJson(obj["leadIn"].toObject());
    if (obj.contains("trailOut"))
        clip.trailOut = transitionFromJson(obj["trailOut"].toObject());

    return clip;
}

// --- Color Correction ---

QJsonObject ProjectFile::colorCorrectionToJson(const ColorCorrection &cc)
{
    QJsonObject obj;
    obj["brightness"] = cc.brightness;
    obj["contrast"] = cc.contrast;
    obj["saturation"] = cc.saturation;
    obj["hue"] = cc.hue;
    obj["temperature"] = cc.temperature;
    obj["tint"] = cc.tint;
    obj["gamma"] = cc.gamma;
    obj["highlights"] = cc.highlights;
    obj["shadows"] = cc.shadows;
    obj["exposure"] = cc.exposure;
    return obj;
}

ColorCorrection ProjectFile::colorCorrectionFromJson(const QJsonObject &obj)
{
    ColorCorrection cc;
    cc.brightness = obj["brightness"].toDouble();
    cc.contrast = obj["contrast"].toDouble();
    cc.saturation = obj["saturation"].toDouble();
    cc.hue = obj["hue"].toDouble();
    cc.temperature = obj["temperature"].toDouble();
    cc.tint = obj["tint"].toDouble();
    cc.gamma = obj["gamma"].toDouble(1.0);
    cc.highlights = obj["highlights"].toDouble();
    cc.shadows = obj["shadows"].toDouble();
    cc.exposure = obj["exposure"].toDouble();
    return cc;
}

// --- Video Effect ---

QJsonObject ProjectFile::effectToJson(const VideoEffect &e)
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(e.type);
    obj["enabled"] = e.enabled;
    obj["param1"] = e.param1;
    obj["param2"] = e.param2;
    obj["param3"] = e.param3;
    obj["keyColor"] = e.keyColor.name();
    return obj;
}

VideoEffect ProjectFile::effectFromJson(const QJsonObject &obj)
{
    VideoEffect e;
    e.type = static_cast<VideoEffectType>(obj["type"].toInt());
    e.enabled = obj["enabled"].toBool(true);
    e.param1 = obj["param1"].toDouble();
    e.param2 = obj["param2"].toDouble();
    e.param3 = obj["param3"].toDouble();
    e.keyColor = QColor(obj["keyColor"].toString("#00ff00"));
    return e;
}

// --- Keyframe Track ---

QJsonObject ProjectFile::keyframeTrackToJson(const KeyframeTrack &track)
{
    QJsonObject obj;
    obj["property"] = track.propertyName();
    obj["defaultValue"] = track.defaultValue();

    QJsonArray kfArr;
    for (const auto &kf : track.keyframes()) {
        QJsonObject kfObj;
        kfObj["time"] = kf.time;
        kfObj["value"] = kf.value;
        kfObj["interpolation"] = static_cast<int>(kf.interpolation);
        kfArr.append(kfObj);
    }
    obj["keyframes"] = kfArr;
    return obj;
}

KeyframeTrack ProjectFile::keyframeTrackFromJson(const QJsonObject &obj)
{
    KeyframeTrack track(obj["property"].toString(), obj["defaultValue"].toDouble());
    for (const auto &v : obj["keyframes"].toArray()) {
        QJsonObject kfObj = v.toObject();
        track.addKeyframe(
            kfObj["time"].toDouble(),
            kfObj["value"].toDouble(),
            static_cast<KeyframePoint::Interpolation>(kfObj["interpolation"].toInt()));
    }
    return track;
}

// --- Keyframe Manager ---

QJsonObject ProjectFile::keyframeManagerToJson(const KeyframeManager &km)
{
    QJsonObject obj;
    QJsonArray tracksArr;
    for (const auto &t : km.tracks())
        tracksArr.append(keyframeTrackToJson(t));
    obj["tracks"] = tracksArr;
    return obj;
}

KeyframeManager ProjectFile::keyframeManagerFromJson(const QJsonObject &obj)
{
    KeyframeManager km;
    for (const auto &v : obj["tracks"].toArray())
        km.addTrack(keyframeTrackFromJson(v.toObject()));
    return km;
}

// --- Transition ---

QJsonObject ProjectFile::transitionToJson(const Transition &t)
{
    QJsonObject obj;
    obj["type"] = static_cast<int>(t.type);
    obj["duration"] = t.duration;
    obj["alignment"] = static_cast<int>(t.alignment);
    obj["easing"] = static_cast<int>(t.easing);
    return obj;
}

Transition ProjectFile::transitionFromJson(const QJsonObject &obj)
{
    Transition t;
    t.type = static_cast<TransitionType>(
        obj["type"].toInt(static_cast<int>(TransitionType::None)));
    t.duration = obj["duration"].toDouble(0.5);
    // Pre-alignment projects default to Center, the pro-NLE default and the
    // alignment that Step 3's overlap math uses when no explicit choice was
    // serialized.
    t.alignment = static_cast<TransitionAlignment>(
        obj["alignment"].toInt(static_cast<int>(TransitionAlignment::Center)));
    // Pre-easing projects default to Linear — the legacy behaviour where
    // every transition advanced its progress with no curve applied.
    t.easing = static_cast<TransitionEasing>(
        obj["easing"].toInt(static_cast<int>(TransitionEasing::Linear)));
    return t;
}

// --- Tracks ---

QJsonArray ProjectFile::tracksToJson(const QVector<QVector<ClipInfo>> &tracks)
{
    QJsonArray arr;
    for (const auto &track : tracks) {
        QJsonArray clipArr;
        for (const auto &clip : track)
            clipArr.append(clipToJson(clip));
        arr.append(clipArr);
    }
    return arr;
}

QVector<QVector<ClipInfo>> ProjectFile::tracksFromJson(const QJsonArray &arr)
{
    QVector<QVector<ClipInfo>> tracks;
    for (const auto &trackVal : arr) {
        QVector<ClipInfo> clips;
        for (const auto &clipVal : trackVal.toArray())
            clips.append(clipFromJson(clipVal.toObject()));
        tracks.append(clips);
    }
    return tracks;
}
