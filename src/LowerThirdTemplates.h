#pragma once
#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVector>

namespace lowerthird {

enum class AnimIn { Slide, Fade, Wipe };

struct LowerThirdStyle {
    QString id;
    QString name;
    QString primaryText;
    QString secondaryText;
    QColor  accentColor;
    QString fontFamily;
    AnimIn  animIn;
    int     durationMs;
};

QVector<LowerThirdStyle> builtInStyles();

// progress in [0,1]: 0 = fully transparent / off-screen, 1 = fully visible.
// Returns an ARGB32 image of `canvas` size with a lower-third bar rendered.
QImage renderFrame(const LowerThirdStyle &style, double progress, const QSize &canvas);

} // namespace lowerthird
