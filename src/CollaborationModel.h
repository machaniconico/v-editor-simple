#pragma once

#include <QString>
#include <QList>
#include <QColor>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
#include <QDateTime>

namespace collab {

enum class Status {
    Open,
    Resolved,
    Deleted
};

inline QString statusToString(Status s) {
    switch (s) {
    case Status::Open:     return QStringLiteral("Open");
    case Status::Resolved: return QStringLiteral("Resolved");
    case Status::Deleted:  return QStringLiteral("Deleted");
    }
    return QStringLiteral("Open");
}

inline Status statusFromString(const QString &s) {
    if (s == QStringLiteral("Resolved")) return Status::Resolved;
    if (s == QStringLiteral("Deleted"))  return Status::Deleted;
    return Status::Open;
}

struct Comment {
    QString  id;
    QString  authorId;
    qint64   timestampMs;  // UTC ms since epoch
    qint64   timecodeMs;   // position within clip in ms
    QString  body;
    QString  parentId;     // empty = top-level
    Status   status;

    QJsonObject toJson() const;
    static Comment fromJson(const QJsonObject &obj);
};

struct User {
    QString id;
    QString displayName;
    QColor  avatarColor;
    QString email;

    QJsonObject toJson() const;
    static User fromJson(const QJsonObject &obj);
};

struct CommentTrack {
    QString         trackId;
    QList<Comment>  comments;
    qint64          version;

    // Mutators — each increments version
    Comment addComment(const QString &authorId, qint64 timecodeMs, const QString &body);
    Comment replyTo(const QString &parentId, const QString &authorId, const QString &body);
    bool    markResolved(const QString &commentId);

    // Queries
    QList<Comment> topLevelComments() const;
    QList<Comment> repliesOf(const QString &parentId) const;

    // Serialization
    QJsonObject toJson() const;
    static CommentTrack fromJson(const QJsonObject &obj);
};

} // namespace collab
