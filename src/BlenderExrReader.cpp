#include "BlenderExrReader.h"

#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

namespace blender::exr {

// ---------------------------------------------------------------------------
// Pattern conversion helpers
// ---------------------------------------------------------------------------

// Convert a glob-like file pattern to a QRegularExpression pattern string.
// Rules (applied in order):
//   '#'  -> \d+   (one or more digits, wrapped in a capture group for frame#)
//   '?'  -> .     (any single character)
//   '*'  -> .*    (any sequence)
//   other metacharacters are escaped with QRegularExpression::escape()
//
// The first '#' sequence is captured as group 1 for frame number extraction.
static QString patternToRegex(const QString &filePattern)
{
    QString regex;
    regex.reserve(filePattern.size() * 2 + 8);

    bool captureAdded = false; // only wrap the first '#' run in a capture group

    for (int i = 0; i < filePattern.size(); ) {
        const QChar ch = filePattern[i];

        if (ch == QLatin1Char('#')) {
            // Consume consecutive '#' characters as a single digit group
            while (i < filePattern.size() && filePattern[i] == QLatin1Char('#'))
                ++i;

            if (!captureAdded) {
                regex += QStringLiteral("(\\d+)");
                captureAdded = true;
            } else {
                regex += QStringLiteral("\\d+");
            }
        } else if (ch == QLatin1Char('?')) {
            regex += QLatin1Char('.');
            ++i;
        } else if (ch == QLatin1Char('*')) {
            regex += QStringLiteral(".*");
            ++i;
        } else {
            // Escape regex metacharacters in the literal portion
            regex += QRegularExpression::escape(QString(ch));
            ++i;
        }
    }

    return regex;
}

// ---------------------------------------------------------------------------
// EXR support probe
// ---------------------------------------------------------------------------

static bool exrSupported()
{
    // Qt6 EXR plugin returns "exr" as the format for files with that extension.
    // We probe with a dummy filename; if the format string is non-empty the
    // plugin is present.
    return !QImageReader::imageFormat("test.exr").isEmpty();
}

// ---------------------------------------------------------------------------
// loadExrSequence
// ---------------------------------------------------------------------------

QList<ExrFrame> loadExrSequence(const QString &folderPath, const QString &filePattern)
{
    QList<ExrFrame> frames;

    // AC-7: non-existent folder
    QDir dir(folderPath);
    if (!dir.exists()) {
        qWarning() << "BlenderExrReader: folder does not exist:" << folderPath;
        return frames;
    }

    const QString regexStr = patternToRegex(filePattern);
    const QRegularExpression re(QStringLiteral("^") + regexStr + QStringLiteral("$"),
                                 QRegularExpression::CaseInsensitiveOption);

    if (!re.isValid()) {
        qWarning() << "BlenderExrReader: invalid regex derived from pattern:"
                   << filePattern << "->" << regexStr;
        return frames;
    }

    const bool canReadExr = exrSupported();

    // Enumerate all files in the folder and match against the derived regex
    const QFileInfoList entries = dir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();
        const QRegularExpressionMatch m = re.match(name);
        if (!m.hasMatch())
            continue;

        ExrFrame frame;
        frame.filename = fi.absoluteFilePath();

        // Extract frame number from capture group 1 (the '#' digits)
        if (m.lastCapturedIndex() >= 1) {
            bool ok = false;
            frame.frameNumber = m.captured(1).toInt(&ok);
            if (!ok)
                frame.frameNumber = 0;
        }

        // AC-6: load image only when EXR plugin is available; otherwise keep isNull()
        if (canReadExr) {
            QImageReader reader(frame.filename);
            frame.image = reader.read();
            if (frame.image.isNull()) {
                qWarning() << "BlenderExrReader: failed to read EXR frame:"
                           << frame.filename << reader.errorString();
            }
        }
        // When !canReadExr, frame.image stays default-constructed (isNull() == true)

        frames.append(frame);
    }

    // AC-5: sort by frameNumber ascending
    std::sort(frames.begin(), frames.end(), [](const ExrFrame &a, const ExrFrame &b) {
        return a.frameNumber < b.frameNumber;
    });

    return frames;
}

} // namespace blender::exr
