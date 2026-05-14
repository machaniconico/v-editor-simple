#include "CollabHistoryLog.h"
#include <QJsonArray>
#include <QUuid>
#include <QDateTime>

namespace collab::history {

QString HistoryLog::addEntry(const QString &authorId,
                              const QString &action,
                              const QString &description,
                              const QString &snapshotHash)
{
    HistoryEntry entry;
    entry.id           = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.timestampMs  = QDateTime::currentMSecsSinceEpoch();
    entry.authorId     = authorId;
    entry.action       = action;
    entry.description  = description;
    entry.snapshotHash = snapshotHash;
    m_entries.append(entry);
    return entry.id;
}

QList<HistoryEntry> HistoryLog::entries() const
{
    return m_entries;
}

std::optional<HistoryEntry> HistoryLog::findById(const QString &id) const
{
    for (const HistoryEntry &e : m_entries) {
        if (e.id == id)
            return e;
    }
    return std::nullopt;
}

QJsonObject HistoryLog::toJson() const
{
    QJsonArray arr;
    for (const HistoryEntry &e : m_entries) {
        QJsonObject obj;
        obj[QStringLiteral("id")]           = e.id;
        obj[QStringLiteral("timestampMs")]  = e.timestampMs;
        obj[QStringLiteral("authorId")]     = e.authorId;
        obj[QStringLiteral("action")]       = e.action;
        obj[QStringLiteral("snapshotHash")] = e.snapshotHash;
        obj[QStringLiteral("description")]  = e.description;
        arr.append(obj);
    }
    QJsonObject root;
    root[QStringLiteral("entries")] = arr;
    return root;
}

HistoryLog HistoryLog::fromJson(const QJsonObject &obj)
{
    HistoryLog log;
    const QJsonArray arr = obj[QStringLiteral("entries")].toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.id           = o[QStringLiteral("id")].toString();
        e.timestampMs  = o[QStringLiteral("timestampMs")].toVariant().toLongLong();
        e.authorId     = o[QStringLiteral("authorId")].toString();
        e.action       = o[QStringLiteral("action")].toString();
        e.snapshotHash = o[QStringLiteral("snapshotHash")].toString();
        e.description  = o[QStringLiteral("description")].toString();
        log.m_entries.append(e);
    }
    return log;
}

} // namespace collab::history
