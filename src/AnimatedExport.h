#pragma once

#include <QImage>
#include <QVector>
#include <QString>
#include <QRgb>

// ---------------------------------------------------------------------------
// Sprint 22 / US-GIF-1 — Animated GIF / WebP exporter
//
// namespace animexport provides:
//   - median-cut color quantization (medianCutPalette)
//   - frame-sequence export to animated GIF89a (hand-built, full LZW encoder)
//     or animated WebP (via QImageWriter when the imageformats plugin allows).
// ---------------------------------------------------------------------------

namespace animexport {

enum class Format { Gif, WebP };

struct ExportConfig {
    Format format  = Format::WebP;
    int    fps     = 15;
    int    width   = 480;
    int    loop    = 0;   // 0 = loop forever
    int    quality = 80;  // WebP quality 0..100
};

// Export a sequence of frames to outPath using cfg.
// Returns false on empty input or unrecoverable write failure.
bool exportFrames(const QVector<QImage> &frames,
                  const QString &outPath,
                  const ExportConfig &cfg);

// Classic median-cut color quantization.
// Returns a palette with size() <= maxColors.
QVector<QRgb> medianCutPalette(const QImage &img, int maxColors = 256);

} // namespace animexport
