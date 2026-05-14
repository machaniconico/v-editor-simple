#pragma once

#include <QString>
#include <QList>
#include <QJsonObject>

#include "CollaborationModel.h"
#include "ProjectFile.h"

namespace collab::share {

struct DiffEntry {
    enum class Type { Added, Removed, Modified };
    Type    type;
    QString path;
    QString description;
};

struct ImportResult {
    ProjectData        project;
    collab::CommentTrack commentTrack;
    QString            warning;
    bool               ok = false;
};

// Export project + comment track to a JSON file at outPath.
// Format: { "project": {...}, "comments": {...}, "history": [], "meta": {...} }
// Returns true on success.
bool exportProject(const ProjectData &project,
                   const collab::CommentTrack &commentTrack,
                   const QString &outPath);

// Import a previously exported file.
// Returns ImportResult with ok=false and a warning on any failure.
ImportResult importProject(const QString &inPath);

// Simple diff: one DiffEntry per category (tracks/clips/effects) where counts differ.
QList<DiffEntry> diffProjects(const ProjectData &a, const ProjectData &b);

} // namespace collab::share
