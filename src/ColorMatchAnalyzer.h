#pragma once

#include <QImage>
#include <QString>
#include <QVector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace colormatch::analyze {

struct ColorStats {
    QVector<int> rHist;  // 256 entries
    QVector<int> gHist;  // 256 entries
    QVector<int> bHist;  // 256 entries
    double rMean    = 0.0;
    double gMean    = 0.0;
    double bMean    = 0.0;
    double rStd     = 0.0;
    double gStd     = 0.0;
    double bStd     = 0.0;
    double luminance = 0.0;  // BT.709: 0.2126*rMean + 0.7152*gMean + 0.0722*bMean
    qint64 sampleCount = 0;

    ColorStats()
        : rHist(256, 0), gHist(256, 0), bHist(256, 0)
    {}
};

// Analyze a single QImage. Returns zero-filled ColorStats if img.isNull().
ColorStats analyzeImage(const QImage &img);

// Decode videoPath with libavformat, iterate frames in [startFrameNo, endFrameNo),
// call analyzeImage on each frame and accumulate pixel-count-weighted stats.
// Returns zero-filled ColorStats on error or empty range; emits qWarning on open failure.
ColorStats analyzeFrameRange(const QString &videoPath, int startFrameNo, int endFrameNo);

} // namespace colormatch::analyze
