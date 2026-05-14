#include "CollaborationModel.h"

namespace collab {

// ---------- helpers ----------------------------------------------------------

static QString newUuid() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

static qint64 nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

// ---------- Comment ----------------------------------------------------------

QJsonObject Comment::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("id")]          = id;
    obj[QStringLiteral("authorId")]    = authorId;
    obj[QStringLiteral("timestampMs")] = timestampMs;
    obj[QStringLiteral("timecodeMs")]  = timecodeMs;
    obj[QStringLiteral("body")]        = body;
    obj[QStringLiteral("parentId")]    = parentId;
    obj[QStringLiteral("status")]      = statusToString(status);
    return obj;
}

Comment Comment::fromJson(const QJsonObject &obj) {
    Comment c;
    c.id          = obj[QStringLiteral("id")].toString();
    c.authorId    = obj[QStringLiteral("authorId")].toString();
    c.timestampMs = static_cast<qint64>(obj[QStringLiteral("timestampMs")].toDouble());
    c.timecodeMs  = static_cast<qint64>(obj[QStringLiteral("timecodeMs")].toDouble());
    c.body        = obj[QStringLiteral("body")].toString();
    c.parentId    = obj[QStringLiteral("parentId")].toString();
    c.status      = statusFromString(obj[QStringLiteral("status")].toString());
    return c;
}

// ---------- User -------------------------------------------------------------

QJsonObject User::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("id")]          = id;
    obj[QStringLiteral("displayName")] = displayName;
    obj[QStringLiteral("avatarColor")] = avatarColor.name(QColor::HexRgb); // #RRGGBB
    obj[QStringLiteral("email")]       = email;
    return obj;
}

User User::fromJson(const QJsonObject &obj) {
    User u;
    u.id          = obj[QStringLiteral("id")].toString();
    u.displayName = obj[QStringLiteral("displayName")].toString();
    u.avatarColor = QColor(obj[QStringLiteral("avatarColor")].toString());
    u.email       = obj[QStringLiteral("email")].toString();
    return u;
}

// ---------- CommentTrack -----------------------------------------------------

Comment CommentTrack::addComment(const QString &authorId,
                                 qint64          timecodeMs,
                                 const QString  &body) {
    Comment c;
    c.id          = newUuid();
    c.authorId    = authorId;
    c.timestampMs = nowMs();
    c.timecodeMs  = timecodeMs;
    c.body        = body;
    c.parentId    = QString(); // top-level
    c.status      = Status::Open;

    comments.append(c);
    ++version;
    return c;
}

Comment CommentTrack::replyTo(const QString &parentId,
                               const QString &authorId,
                               const QString &body) {
    Comment c;
    c.id          = newUuid();
    c.authorId    = authorId;
    c.timestampMs = nowMs();
    c.timecodeMs  = 0; // replies inherit context from parent
    c.body        = body;
    c.parentId    = parentId;
    c.status      = Status::Open;

    comments.append(c);
    ++version;
    return c;
}

bool CommentTrack::markResolved(const QString &commentId) {
    for (Comment &c : comments) {
        if (c.id == commentId) {
            if (c.status == Status::Deleted) return false;
            c.status = Status::Resolved;
            ++version;
            return true;
        }
    }
    return false;
}

QList<Comment> CommentTrack::topLevelComments() const {
    QList<Comment> result;
    for (const Comment &c : comments) {
        if (c.parentId.isEmpty()) {
            result.append(c);
        }
    }
    return result;
}

QList<Comment> CommentTrack::repliesOf(const QString &parentId) const {
    QList<Comment> result;
    for (const Comment &c : comments) {
        if (c.parentId == parentId) {
            result.append(c);
        }
    }
    return result;
}

QJsonObject CommentTrack::toJson() const {
    QJsonArray arr;
    for (const Comment &c : comments) {
        arr.append(c.toJson());
    }
    QJsonObject obj;
    obj[QStringLiteral("trackId")]  = trackId;
    obj[QStringLiteral("comments")] = arr;
    obj[QStringLiteral("version")]  = version;
    return obj;
}

CommentTrack CommentTrack::fromJson(const QJsonObject &obj) {
    CommentTrack t;
    t.trackId = obj[QStringLiteral("trackId")].toString();
    t.version = static_cast<qint64>(obj[QStringLiteral("version")].toDouble());

    const QJsonArray arr = obj[QStringLiteral("comments")].toArray();
    t.comments.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        t.comments.append(Comment::fromJson(v.toObject()));
    }
    return t;
}

} // namespace collab
