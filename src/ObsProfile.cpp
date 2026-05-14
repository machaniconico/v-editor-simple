#include "ObsProfile.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

namespace obs::profile {

QList<SceneInfo> loadSceneCollection(const QString &jsonPath)
{
    QList<SceneInfo> result;

    QFile file(jsonPath);
    if (!file.exists()) {
        qWarning() << "ObsProfile: file does not exist:" << jsonPath;
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ObsProfile: cannot open file:" << jsonPath;
        return result;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ObsProfile: JSON parse error:" << parseError.errorString();
        return result;
    }
    if (!doc.isObject()) {
        qWarning() << "ObsProfile: root is not a JSON object";
        return result;
    }

    QJsonObject root = doc.object();

    // Collect ordered scene names from scene_order[]
    QStringList sceneOrder;
    QJsonArray sceneOrderArr = root.value(QStringLiteral("scene_order")).toArray();
    for (const QJsonValue &v : sceneOrderArr) {
        QString name = v.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            sceneOrder.append(name);
    }

    // Build a map from scene name -> SceneInfo by walking sources[]
    QHash<QString, SceneInfo> sceneMap;

    QJsonArray sources = root.value(QStringLiteral("sources")).toArray();
    for (const QJsonValue &sv : sources) {
        QJsonObject src = sv.toObject();
        if (src.value(QStringLiteral("id")).toString() != QStringLiteral("scene"))
            continue;

        QString sceneName = src.value(QStringLiteral("name")).toString();
        if (sceneName.isEmpty())
            continue;

        SceneInfo info;
        info.name = sceneName;

        // Enumerate child sources from settings.items[]
        QJsonObject settings = src.value(QStringLiteral("settings")).toObject();
        QJsonArray items = settings.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &item : items) {
            QString sourceName = item.toObject().value(QStringLiteral("name")).toString();
            if (!sourceName.isEmpty())
                info.sources.append(sourceName);
        }

        // Collect audio track names (mixers bitmask field is not a name list;
        // use sources whose id indicates audio capture where available)
        // For v28+ there is no explicit audioTracks[] at scene level;
        // we leave audioTracks empty here (caller may populate from RecordingGroup).
        // If a future schema adds it, extend here.

        sceneMap.insert(sceneName, info);
    }

    // Return scenes in scene_order sequence; unknown names get an empty SceneInfo
    for (const QString &name : sceneOrder) {
        if (sceneMap.contains(name)) {
            result.append(sceneMap.value(name));
        } else {
            SceneInfo empty;
            empty.name = name;
            result.append(empty);
        }
    }

    // If scene_order was empty but we found scene sources, return those
    if (result.isEmpty() && !sceneMap.isEmpty()) {
        result = sceneMap.values();
    }

    return result;
}

} // namespace obs::profile
