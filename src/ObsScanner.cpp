#include "ObsScanner.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>

extern "C" {
#include <libavformat/avformat.h>
}

namespace obs::scan {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

// Timestamp pattern: "YYYY-MM-DD HH-MM-SS"
static const QRegularExpression kTimestampRe(
    QStringLiteral(R"((\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2}))"));

// Audio track suffix: "<stem>-trackN.<ext>"
static const QRegularExpression kAudioTrackRe(
    QStringLiteral(R"(^(.+)-track(\d+)\.(aac|m4a)$)"),
    QRegularExpression::CaseInsensitiveOption);

// Replay buffer: starts with "Replay "
static const QRegularExpression kReplayRe(
    QStringLiteral(R"(^Replay \d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2})"),
    QRegularExpression::CaseInsensitiveOption);

// Source Record: "<SourceName>_YYYY-MM-DD HH-MM-SS"
// Must contain an underscore followed by the timestamp pattern.
static const QRegularExpression kSourceRecordRe(
    QStringLiteral(R"(^.+_\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2})"));

// OBS standard: filename IS "YYYY-MM-DD HH-MM-SS" (possibly with extension).
static const QRegularExpression kObsStandardRe(
    QStringLiteral(R"(^\d{4}-\d{2}-\d{2} \d{2}-\d{2}-\d{2}$)"));

enum class FileKind {
    AudioTrack,
    ReplayBuffer,
    SourceRecord,
    ObsStandard,
    Unknown
};

static const QStringList kVideoExts = { "mkv", "mp4", "flv" };
static const QStringList kAudioExts = { "aac", "m4a" };

bool isVideoExt(const QString &ext)
{
    return kVideoExts.contains(ext, Qt::CaseInsensitive);
}

bool isAudioExt(const QString &ext)
{
    return kAudioExts.contains(ext, Qt::CaseInsensitive);
}

// Classify a file given its full filename (including extension suffix check).
FileKind classifyFile(const QFileInfo &fi, QString &audioStem)
{
    const QString ext = fi.suffix().toLower();
    const QString base = fi.completeBaseName(); // name without last extension

    // Audio track files (aac / m4a)
    if (isAudioExt(ext)) {
        auto m = kAudioTrackRe.match(fi.fileName());
        if (m.hasMatch()) {
            audioStem = m.captured(1);
            return FileKind::AudioTrack;
        }
        return FileKind::Unknown;
    }

    if (!isVideoExt(ext))
        return FileKind::Unknown;

    // Video file — classify by base name (priority order)
    if (kReplayRe.match(base).hasMatch())
        return FileKind::ReplayBuffer;
    if (kSourceRecordRe.match(base).hasMatch())
        return FileKind::SourceRecord;
    if (kObsStandardRe.match(base).hasMatch())
        return FileKind::ObsStandard;

    return FileKind::Unknown;
}

// Parse a QDateTime from the timestamp embedded in the filename.
QDateTime parseTimestamp(const QString &name)
{
    auto m = kTimestampRe.match(name);
    if (!m.hasMatch())
        return QDateTime();
    // "YYYY-MM-DD HH-MM-SS" → replace second and third '-' in time part
    QString ts = m.captured(1);
    // ts[10] == ' ', ts[13] == '-', ts[16] == '-'
    ts[13] = ':';
    ts[16] = ':';
    return QDateTime::fromString(ts, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

// Use libavformat to probe duration (seconds). Returns 0.0 on failure.
double probeDuration(const QString &path)
{
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.toUtf8().constData(), nullptr, nullptr) < 0)
        return 0.0;
    double dur = 0.0;
    if (avformat_find_stream_info(fmtCtx, nullptr) >= 0) {
        if (fmtCtx->duration != AV_NOPTS_VALUE)
            dur = static_cast<double>(fmtCtx->duration) / AV_TIME_BASE;
    }
    avformat_close_input(&fmtCtx);
    return dur;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QList<RecordingGroup> scanFolder(const QString &folderPath)
{
    // Maps: sessionId (stem) → RecordingGroup
    QMap<QString, RecordingGroup> groups;

    QDirIterator it(folderPath,
                    QStringList() << "*.mkv" << "*.mp4" << "*.flv"
                                  << "*.aac" << "*.m4a",
                    QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString filePath = it.next();
        const QFileInfo fi(filePath);

        QString audioStem;
        const FileKind kind = classifyFile(fi, audioStem);

        if (kind == FileKind::Unknown)
            continue;

        if (kind == FileKind::AudioTrack) {
            // Attach to the video group identified by stem, or create a stub group.
            if (!groups.contains(audioStem)) {
                RecordingGroup g;
                g.sessionId = audioStem;
                g.startedAt = parseTimestamp(audioStem);
                groups.insert(audioStem, g);
            }
            groups[audioStem].audioTrackFiles.append(filePath);
            continue;
        }

        // Video file — derive session key from base name
        const QString base = fi.completeBaseName();

        if (!groups.contains(base)) {
            RecordingGroup g;
            g.sessionId = base;
            g.startedAt = parseTimestamp(base);
            groups.insert(base, g);
        }

        RecordingGroup &g = groups[base];

        switch (kind) {
        case FileKind::ObsStandard:
            if (g.primaryVideoFile.isEmpty()) {
                g.primaryVideoFile = filePath;
                if (g.durationSec == 0.0)
                    g.durationSec = probeDuration(filePath);
            }
            break;
        case FileKind::ReplayBuffer:
            g.replayBufferFiles.append(filePath);
            break;
        case FileKind::SourceRecord:
            g.sourceRecordFiles.append(filePath);
            break;
        default:
            break;
        }
    }

    return groups.values();
}

} // namespace obs::scan
