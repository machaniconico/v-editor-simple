#include "LowerThirdTemplates.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>

namespace lowerthird {

// ---------------------------------------------------------------------------
// builtInStyles — 10 distinct presets
// ---------------------------------------------------------------------------
QVector<LowerThirdStyle> builtInStyles()
{
    QVector<LowerThirdStyle> s;

    s.push_back({ QStringLiteral("news"),      QStringLiteral("News"),
                  QStringLiteral("Breaking News"),   QStringLiteral("Live Coverage"),
                  QColor(0xCC, 0x00, 0x00), QStringLiteral("Arial"),
                  AnimIn::Slide, 800 });

    s.push_back({ QStringLiteral("corporate"), QStringLiteral("Corporate"),
                  QStringLiteral("Jane Smith"),      QStringLiteral("Chief Executive Officer"),
                  QColor(0x00, 0x4C, 0x97), QStringLiteral("Segoe UI"),
                  AnimIn::Fade, 600 });

    s.push_back({ QStringLiteral("minimal"),   QStringLiteral("Minimal"),
                  QStringLiteral("John Doe"),        QStringLiteral("Speaker"),
                  QColor(0xF0, 0xF0, 0xF0), QStringLiteral("Helvetica Neue"),
                  AnimIn::Wipe, 500 });

    s.push_back({ QStringLiteral("bold"),      QStringLiteral("Bold"),
                  QStringLiteral("BIG TITLE"),       QStringLiteral("Supporting Text"),
                  QColor(0xFF, 0x6B, 0x00), QStringLiteral("Impact"),
                  AnimIn::Slide, 700 });

    s.push_back({ QStringLiteral("gradient"),  QStringLiteral("Gradient"),
                  QStringLiteral("Gradient Style"),  QStringLiteral("Dynamic Lower Third"),
                  QColor(0x6A, 0x0D, 0xAD), QStringLiteral("Trebuchet MS"),
                  AnimIn::Fade, 900 });

    s.push_back({ QStringLiteral("neon"),      QStringLiteral("Neon"),
                  QStringLiteral("NEON TITLE"),      QStringLiteral("Electric Subtitle"),
                  QColor(0x39, 0xFF, 0x14), QStringLiteral("Courier New"),
                  AnimIn::Wipe, 600 });

    s.push_back({ QStringLiteral("elegant"),   QStringLiteral("Elegant"),
                  QStringLiteral("Elegant Title"),   QStringLiteral("Fine Subtitle"),
                  QColor(0xC9, 0xA0, 0x4A), QStringLiteral("Georgia"),
                  AnimIn::Fade, 1000 });

    s.push_back({ QStringLiteral("tech"),      QStringLiteral("Tech"),
                  QStringLiteral("TECH REPORT"),     QStringLiteral("Data & Analysis"),
                  QColor(0x00, 0xD4, 0xFF), QStringLiteral("Consolas"),
                  AnimIn::Slide, 500 });

    s.push_back({ QStringLiteral("playful"),   QStringLiteral("Playful"),
                  QStringLiteral("Hello World!"),    QStringLiteral("Fun & Friendly"),
                  QColor(0xFF, 0x45, 0x93), QStringLiteral("Comic Sans MS"),
                  AnimIn::Wipe, 700 });

    s.push_back({ QStringLiteral("cinematic"), QStringLiteral("Cinematic"),
                  QStringLiteral("CINEMATIC TITLE"), QStringLiteral("A Feature Presentation"),
                  QColor(0xD4, 0xAF, 0x37), QStringLiteral("Palatino Linotype"),
                  AnimIn::Fade, 1200 });

    return s;
}

// ---------------------------------------------------------------------------
// renderFrame
// ---------------------------------------------------------------------------
QImage renderFrame(const LowerThirdStyle &style, double progress, const QSize &canvas)
{
    // Clamp progress to [0,1]
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    QImage img(canvas, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    if (progress <= 0.0)
        return img;

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Layout constants (relative to canvas)
    const int margin       = canvas.width()  / 20;       // left margin
    const int barY         = canvas.height() * 2 / 3;    // top of bar (lower third)
    const int barHeight    = canvas.height() / 5;
    const int barWidth     = canvas.width()  * 55 / 100; // ~55% of width
    const int accentW      = 6;                           // accent stripe width

    // Font sizes
    const int primarySize  = barHeight * 38 / 100;
    const int secondarySize = barHeight * 24 / 100;

    // Primary text to render
    const QString primary   = style.primaryText.isEmpty()
                              ? QStringLiteral("Primary Text") : style.primaryText;
    const QString secondary = style.secondaryText.isEmpty()
                              ? QStringLiteral("Secondary Text") : style.secondaryText;

    // -----------------------------------------------------------------
    // Apply AnimIn mode
    // -----------------------------------------------------------------
    switch (style.animIn)
    {
    case AnimIn::Slide:
    {
        // Bar slides in from the left: x starts off-screen, ends at margin
        const int startX = -barWidth;
        const int endX   = margin;
        const int currentX = static_cast<int>(startX + (endX - startX) * progress);

        // Draw background bar
        QRect barRect(currentX, barY, barWidth, barHeight);
        p.fillRect(barRect, QColor(0, 0, 0, 180));

        // Accent stripe
        QRect accentRect(currentX, barY, accentW, barHeight);
        p.fillRect(accentRect, style.accentColor);

        // Primary text
        QFont pFont(style.fontFamily);
        pFont.setPixelSize(primarySize);
        pFont.setBold(true);
        p.setFont(pFont);
        p.setPen(Qt::white);
        QRect primaryRect(currentX + accentW + 8, barY + 4,
                          barWidth - accentW - 16, barHeight / 2);
        p.drawText(primaryRect, Qt::AlignLeft | Qt::AlignVCenter, primary);

        // Secondary text
        QFont sFont(style.fontFamily);
        sFont.setPixelSize(secondarySize);
        p.setFont(sFont);
        p.setPen(QColor(220, 220, 220));
        QRect secondaryRect(currentX + accentW + 8, barY + barHeight / 2,
                            barWidth - accentW - 16, barHeight / 2 - 4);
        p.drawText(secondaryRect, Qt::AlignLeft | Qt::AlignVCenter, secondary);
        break;
    }

    case AnimIn::Fade:
    {
        // Whole group fades in: alpha = progress
        const qreal alpha = progress;
        p.setOpacity(alpha);

        QRect barRect(margin, barY, barWidth, barHeight);
        p.fillRect(barRect, QColor(0, 0, 0, 180));

        QRect accentRect(margin, barY, accentW, barHeight);
        p.fillRect(accentRect, style.accentColor);

        QFont pFont(style.fontFamily);
        pFont.setPixelSize(primarySize);
        pFont.setBold(true);
        p.setFont(pFont);
        p.setPen(Qt::white);
        QRect primaryRect(margin + accentW + 8, barY + 4,
                          barWidth - accentW - 16, barHeight / 2);
        p.drawText(primaryRect, Qt::AlignLeft | Qt::AlignVCenter, primary);

        QFont sFont(style.fontFamily);
        sFont.setPixelSize(secondarySize);
        p.setFont(sFont);
        p.setPen(QColor(220, 220, 220));
        QRect secondaryRect(margin + accentW + 8, barY + barHeight / 2,
                            barWidth - accentW - 16, barHeight / 2 - 4);
        p.drawText(secondaryRect, Qt::AlignLeft | Qt::AlignVCenter, secondary);

        p.setOpacity(1.0);
        break;
    }

    case AnimIn::Wipe:
    {
        // Clip rect width grows from 0 to barWidth as progress goes 0→1
        const int clipW = static_cast<int>(barWidth * progress);
        if (clipW <= 0)
            break;

        p.setClipRect(QRect(margin, barY, clipW, barHeight));

        QRect barRect(margin, barY, barWidth, barHeight);
        p.fillRect(barRect, QColor(0, 0, 0, 180));

        QRect accentRect(margin, barY, accentW, barHeight);
        p.fillRect(accentRect, style.accentColor);

        QFont pFont(style.fontFamily);
        pFont.setPixelSize(primarySize);
        pFont.setBold(true);
        p.setFont(pFont);
        p.setPen(Qt::white);
        QRect primaryRect(margin + accentW + 8, barY + 4,
                          barWidth - accentW - 16, barHeight / 2);
        p.drawText(primaryRect, Qt::AlignLeft | Qt::AlignVCenter, primary);

        QFont sFont(style.fontFamily);
        sFont.setPixelSize(secondarySize);
        p.setFont(sFont);
        p.setPen(QColor(220, 220, 220));
        QRect secondaryRect(margin + accentW + 8, barY + barHeight / 2,
                            barWidth - accentW - 16, barHeight / 2 - 4);
        p.drawText(secondaryRect, Qt::AlignLeft | Qt::AlignVCenter, secondary);

        p.setClipping(false);
        break;
    }
    }

    p.end();
    return img;
}

} // namespace lowerthird
