#include "Overlay.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

void OverlayRenderer::renderTextOverlay(QImage &frame, const TextOverlay &overlay, double currentTime)
{
    if (!overlay.visible) return;
    if (currentTime < overlay.startTime) return;
    if (overlay.endTime > 0 && currentTime > overlay.endTime) return;

    QPainter painter(&frame);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.setFont(overlay.font);
    QFontMetrics fm(overlay.font);
    QRect textBounds = fm.boundingRect(QRect(0, 0, frame.width() - 40, 0),
        overlay.alignment | Qt::TextWordWrap, overlay.text);

    int px = static_cast<int>(overlay.x * frame.width()) - textBounds.width() / 2;
    int py = static_cast<int>(overlay.y * frame.height()) - textBounds.height() / 2;
    QRect drawRect(px, py, textBounds.width(), textBounds.height());

    // Background
    if (overlay.backgroundColor.alpha() > 0) {
        painter.fillRect(drawRect.adjusted(-8, -4, 8, 4), overlay.backgroundColor);
    }

    // Outline
    if (overlay.outlineWidth > 0) {
        QPainterPath path;
        path.addText(px, py + fm.ascent(), overlay.font, overlay.text);
        painter.setPen(QPen(overlay.outlineColor, overlay.outlineWidth));
        painter.drawPath(path);
    }

    // Text
    painter.setPen(overlay.color);
    painter.drawText(drawRect, overlay.alignment | Qt::TextWordWrap, overlay.text);
}

void OverlayRenderer::renderImageOverlay(QImage &frame, const ImageOverlay &overlay, double currentTime)
{
    if (!overlay.visible) return;
    if (currentTime < overlay.startTime) return;
    if (overlay.endTime > 0 && currentTime > overlay.endTime) return;

    QImage img(overlay.filePath);
    if (img.isNull()) return;

    int x = static_cast<int>(overlay.rect.x() * frame.width());
    int y = static_cast<int>(overlay.rect.y() * frame.height());
    int w = static_cast<int>(overlay.rect.width() * frame.width());
    int h = static_cast<int>(overlay.rect.height() * frame.height());

    QImage scaled;
    if (overlay.keepAspectRatio)
        scaled = img.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    else
        scaled = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&frame);
    painter.setOpacity(overlay.opacity);
    painter.drawImage(x, y, scaled);
}

void OverlayRenderer::renderPip(QImage &frame, const QImage &pipSource, const PipConfig &config)
{
    if (!config.visible || pipSource.isNull()) return;

    int x = static_cast<int>(config.rect.x() * frame.width());
    int y = static_cast<int>(config.rect.y() * frame.height());
    int w = static_cast<int>(config.rect.width() * frame.width());
    int h = static_cast<int>(config.rect.height() * frame.height());

    QImage scaled = pipSource.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QPainter painter(&frame);
    painter.setOpacity(config.opacity);

    // Border
    if (config.borderWidth > 0) {
        painter.setPen(QPen(config.borderColor, config.borderWidth));
        painter.drawRect(x - 1, y - 1, scaled.width() + 1, scaled.height() + 1);
    }

    painter.drawImage(x, y, scaled);
}

QImage OverlayRenderer::applyTransition(const QImage &from, const QImage &to,
    TransitionType type, double progress)
{
    if (type == TransitionType::None) return (progress < 0.5) ? from : to;

    int w = qMax(from.width(), to.width());
    int h = qMax(from.height(), to.height());
    QImage result(w, h, QImage::Format_RGB888);
    QPainter painter(&result);

    QImage fromScaled = from.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage toScaled = to.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    switch (type) {
    case TransitionType::FadeIn:
        painter.drawImage(0, 0, fromScaled);
        painter.setOpacity(progress);
        painter.drawImage(0, 0, toScaled);
        break;

    case TransitionType::FadeOut:
        painter.drawImage(0, 0, toScaled);
        painter.setOpacity(1.0 - progress);
        painter.drawImage(0, 0, fromScaled);
        break;

    case TransitionType::CrossDissolve:
        painter.setOpacity(1.0 - progress);
        painter.drawImage(0, 0, fromScaled);
        painter.setOpacity(progress);
        painter.drawImage(0, 0, toScaled);
        break;

    case TransitionType::WipeLeft: {
        int boundary = static_cast<int>(w * progress);
        painter.drawImage(0, 0, fromScaled);
        painter.setClipRect(0, 0, boundary, h);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::WipeRight: {
        int boundary = static_cast<int>(w * (1.0 - progress));
        painter.drawImage(0, 0, fromScaled);
        painter.setClipRect(boundary, 0, w - boundary, h);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::WipeUp: {
        int boundary = static_cast<int>(h * progress);
        painter.drawImage(0, 0, fromScaled);
        painter.setClipRect(0, 0, w, boundary);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::WipeDown: {
        int boundary = static_cast<int>(h * (1.0 - progress));
        painter.drawImage(0, 0, fromScaled);
        painter.setClipRect(0, boundary, w, h - boundary);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::SlideLeft: {
        int offset = static_cast<int>(w * progress);
        painter.drawImage(-offset, 0, fromScaled);
        painter.drawImage(w - offset, 0, toScaled);
        break;
    }

    case TransitionType::SlideRight: {
        int offset = static_cast<int>(w * progress);
        painter.drawImage(offset, 0, fromScaled);
        painter.drawImage(-w + offset, 0, toScaled);
        break;
    }

    case TransitionType::SlideUp: {
        int offset = static_cast<int>(h * progress);
        painter.drawImage(0, -offset, fromScaled);
        painter.drawImage(0, h - offset, toScaled);
        break;
    }

    case TransitionType::SlideDown: {
        int offset = static_cast<int>(h * progress);
        painter.drawImage(0, offset, fromScaled);
        painter.drawImage(0, -h + offset, toScaled);
        break;
    }

    case TransitionType::DipToBlack:
    case TransitionType::DipToWhite: {
        // First half: A fades to black/white. Second half: black/white fades
        // to B. Symmetric — at progress=0.5 the frame is solid black/white.
        const QColor mid = (type == TransitionType::DipToBlack)
                           ? QColor(0, 0, 0) : QColor(255, 255, 255);
        painter.fillRect(0, 0, w, h, mid);
        if (progress < 0.5) {
            painter.setOpacity(1.0 - progress * 2.0);
            painter.drawImage(0, 0, fromScaled);
        } else {
            painter.setOpacity((progress - 0.5) * 2.0);
            painter.drawImage(0, 0, toScaled);
        }
        break;
    }

    case TransitionType::IrisRound: {
        // Circular iris expanding from the centre — B revealed inside the
        // growing disc.
        painter.drawImage(0, 0, fromScaled);
        const int cx = w / 2;
        const int cy = h / 2;
        const double maxR = std::sqrt(double(cx) * cx + double(cy) * cy) + 2.0;
        const double r = maxR * progress;
        QPainterPath path;
        path.addEllipse(QPointF(cx, cy), r, r);
        painter.setClipPath(path);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::IrisBox: {
        // Rectangular iris expanding from the centre.
        painter.drawImage(0, 0, fromScaled);
        const int rectW = static_cast<int>(w * progress);
        const int rectH = static_cast<int>(h * progress);
        const int rx = (w - rectW) / 2;
        const int ry = (h - rectH) / 2;
        painter.setClipRect(rx, ry, rectW, rectH);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::ClockWipe: {
        // Radial sweep clockwise from 12 o'clock. Skip the pie-path entirely
        // at progress >= 1 so the closeSubpath()'s radial line back to the
        // center doesn't leave a 1-pixel seam at the final frame.
        if (progress >= 1.0) {
            painter.drawImage(0, 0, toScaled);
            break;
        }
        painter.drawImage(0, 0, fromScaled);
        const int cx = w / 2;
        const int cy = h / 2;
        const double maxR = std::sqrt(double(cx) * cx + double(cy) * cy) + 2.0;
        QPainterPath path;
        path.moveTo(cx, cy);
        // Qt arc angles: 0° at 3 o'clock, positive = counter-clockwise.
        // Start at 90° (12 o'clock) and sweep negative for clockwise.
        path.arcTo(cx - maxR, cy - maxR, maxR * 2, maxR * 2,
                   90.0, -360.0 * progress);
        path.closeSubpath();
        painter.setClipPath(path);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::BarnDoorHorizontal: {
        // Two doors opening horizontally from the centre, revealing B
        // through the widening gap.
        painter.drawImage(0, 0, fromScaled);
        const int strip = static_cast<int>((w / 2) * progress);
        const int cx = w / 2;
        painter.setClipRect(cx - strip, 0, strip * 2, h);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    case TransitionType::BarnDoorVertical: {
        painter.drawImage(0, 0, fromScaled);
        const int strip = static_cast<int>((h / 2) * progress);
        const int cy = h / 2;
        painter.setClipRect(0, cy - strip, w, strip * 2);
        painter.drawImage(0, 0, toScaled);
        break;
    }

    default:
        painter.drawImage(0, 0, (progress < 0.5) ? fromScaled : toScaled);
        break;
    }

    return result;
}
