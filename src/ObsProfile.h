#pragma once

#include <QString>
#include <QList>

namespace obs::profile {

struct SceneInfo {
    QString         name;
    QStringList     sources;
    QStringList     audioTracks;
};

// Parse an OBS scene-collection JSON file (v28+ format).
// Returns one SceneInfo per scene found in scene_order[].
// Returns an empty list if jsonPath does not exist or JSON is malformed.
// Never throws.
QList<SceneInfo> loadSceneCollection(const QString &jsonPath);

} // namespace obs::profile
