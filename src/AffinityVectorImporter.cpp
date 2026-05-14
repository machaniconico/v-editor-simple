#include "AffinityVectorImporter.h"

#include <QtSvg/QSvgRenderer>
#include <QPainter>
#include <QImageReader>
#include <QFileInfo>
#include <QDebug>

#if __has_include(<QtPdf/QPdfDocument>)
#  include <QtPdf/QPdfDocument>
#  define AFFINITY_HAS_QTPDF 1
#else
#  define AFFINITY_HAS_QTPDF 0
#endif

namespace affinity::vector {

QImage loadSvg(const QString &path, QSize targetRender)
{
    if (!QFileInfo::exists(path)) {
        qWarning() << "[affinity::vector] loadSvg: file not found:" << path;
        return QImage();
    }

    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        qWarning() << "[affinity::vector] loadSvg: invalid SVG:" << path;
        return QImage();
    }

    QSize renderSize = targetRender;
    if (renderSize.isEmpty()) {
        // Use the SVG's natural viewBox size
        renderSize = renderer.defaultSize();
    }

    if (renderSize.isEmpty()) {
        qWarning() << "[affinity::vector] loadSvg: SVG has no size:" << path;
        return QImage();
    }

    QImage image(renderSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();

    return image;
}

QImage loadPdf(const QString &path, int pageIndex, int targetDpi)
{
#if AFFINITY_HAS_QTPDF
    if (!QFileInfo::exists(path)) {
        qWarning() << "[affinity::vector] loadPdf: file not found:" << path;
        return QImage();
    }

    QPdfDocument doc;
    if (doc.load(path) != QPdfDocument::Status::Ready) {
        qWarning() << "[affinity::vector] loadPdf: failed to load PDF:" << path;
        return QImage();
    }

    if (pageIndex < 0 || pageIndex >= doc.pageCount()) {
        qWarning() << "[affinity::vector] loadPdf: invalid pageIndex" << pageIndex
                   << "for" << path;
        return QImage();
    }

    // PDF points are at 72 dpi; scale to targetDpi
    const double scale = static_cast<double>(targetDpi) / 72.0;
    const QSizeF pageSize = doc.pagePointSize(pageIndex);
    const QSize renderSize(
        static_cast<int>(pageSize.width()  * scale),
        static_cast<int>(pageSize.height() * scale)
    );

    return doc.render(pageIndex, renderSize);
#else
    Q_UNUSED(path)
    Q_UNUSED(pageIndex)
    Q_UNUSED(targetDpi)
    qWarning() << "[affinity::vector] loadPdf: QtPdf not available in this build";
    return QImage();
#endif
}

QImage loadTiff(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        qWarning() << "[affinity::vector] loadTiff: file not found:" << path;
        return QImage();
    }

    QImageReader reader(path);
    reader.setFormat("tiff");

    if (!reader.canRead()) {
        qWarning() << "[affinity::vector] loadTiff: cannot read TIFF:" << path;
        return QImage();
    }

    QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "[affinity::vector] loadTiff: failed to read TIFF:" << path
                   << reader.errorString();
    }
    return image;
}

QList<QImage> loadTiffAllPages(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        qWarning() << "[affinity::vector] loadTiffAllPages: file not found:" << path;
        return {};
    }

    QImageReader reader(path);
    reader.setFormat("tiff");

    if (!reader.canRead()) {
        qWarning() << "[affinity::vector] loadTiffAllPages: cannot read TIFF:" << path;
        return {};
    }

    const int pageCount = reader.imageCount();
    QList<QImage> pages;
    pages.reserve(pageCount > 0 ? pageCount : 1);

    if (pageCount <= 0) {
        // Some implementations report 0; try reading a single page
        QImage img = reader.read();
        if (!img.isNull()) {
            pages.append(img);
        } else {
            qWarning() << "[affinity::vector] loadTiffAllPages: failed to read:"
                       << path << reader.errorString();
        }
        return pages;
    }

    for (int i = 0; i < pageCount; ++i) {
        if (!reader.jumpToImage(i)) {
            qWarning() << "[affinity::vector] loadTiffAllPages: jumpToImage failed for page"
                       << i << "in" << path;
            continue;
        }
        QImage img = reader.read();
        if (img.isNull()) {
            qWarning() << "[affinity::vector] loadTiffAllPages: failed to read page"
                       << i << "in" << path << reader.errorString();
        } else {
            pages.append(img);
        }
    }

    return pages;
}

} // namespace affinity::vector
