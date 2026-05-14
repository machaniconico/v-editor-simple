#pragma once

#include <QImage>
#include <QList>
#include <QString>

namespace blender::exr {

// A single frame from an EXR image sequence
struct ExrFrame {
    int     frameNumber = 0;    // parsed from filename capture group
    QImage  image;              // decoded QImage; isNull() when EXR plugin unavailable
    QString filename;           // absolute file path
};

// Load an EXR image sequence from folderPath.
//
// filePattern uses glob-like syntax:
//   '#'  -> one or more digits  (e.g. 'render_####.exr' -> 'render_(\\d+)\\.exr')
//   '?'  -> any single character
//   '*'  -> any sequence of characters
//
// Returns frames sorted by frameNumber ascending.
// If the folder does not exist, returns an empty list and logs a warning.
// If QImageReader cannot handle EXR on this platform, each ExrFrame is
// returned with image.isNull() == true but frameNumber and filename filled in.
QList<ExrFrame> loadExrSequence(const QString &folderPath, const QString &filePattern);

} // namespace blender::exr
