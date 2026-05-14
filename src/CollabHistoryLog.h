#pragma once

#include "CollaborationModel.h"
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>
#include <optional>

namespace collab::history {

struct HistoryEntry {
    QString id;           // UUID
    qint64  timestampMs;  // UTC ms since epoch
    QString authorId;
    QString action;
    QString snapshotHash;
    QString description;
};

class HistoryLog {
public:
    HistoryLog() = default;

    // Adds a new entry; returns the generated entry id (UUID)
    QString addEntry(const QString &authorId,
                     const QString &action,
                     const QString &description,
                     const QString &snapshotHash);

    QList<HistoryEntry> entries() const;

    std::optional<HistoryEntry> findById(const QString &id) const;

    QJsonObject toJson() const;
    static HistoryLog fromJson(const QJsonObject &obj);

private:
    QList<HistoryEntry> m_entries;
};

} // namespace collab::history
