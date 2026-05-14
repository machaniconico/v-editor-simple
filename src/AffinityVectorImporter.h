#pragma once

#include <QString>
#include <QImage>
#include <QList>
#include <QSize>

namespace affinity::vector {

// Load an SVG file and rasterize to QImage.
// If targetRender is empty (isNull/isEmpty), use the SVG's natural viewBox size.
// Returns null QImage on failure (no throw).
QImage loadSvg(const QString &path, QSize targetRender = QSize());

// Load a single page from a PDF file at the given DPI.
// pageIndex is 0-based. targetDpi uses 72 dpi as PDF baseline.
// Requires Qt6.5+ QPdfDocument; otherwise returns null QImage stub.
// Returns null QImage on failure (no throw).
QImage loadPdf(const QString &path, int pageIndex = 0, int targetDpi = 96);

// Load the first (or only) page of a TIFF file.
// Returns null QImage on failure (no throw).
QImage loadTiff(const QString &path);

// Load all pages of a (possibly multi-page) TIFF file.
// Returns a list with at least 1 element for a valid single-page TIFF.
// Returns empty list on failure (no throw).
QList<QImage> loadTiffAllPages(const QString &path);

} // namespace affinity::vector
