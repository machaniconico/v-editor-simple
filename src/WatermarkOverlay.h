#pragma once
#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>

namespace watermark {

enum class Mode {
    Image,
    Text
};

enum class Position {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center,
    Tiled
};

struct WmConfig {
    Mode     mode        = Mode::Text;
    QString  imagePath;
    QString  text        = QStringLiteral("© Sample");
    Position position    = Position::BottomRight;
    double   opacity     = 0.5;
    double   scale       = 0.15;
    int      marginPx    = 24;
    double   rotationDeg = 0.0;
    QColor   textColor   = Qt::white;
};

QImage applyWatermark(const QImage &frame, const WmConfig &cfg);
int    batchApply(const QStringList &inputPaths, const QString &outDir, const WmConfig &cfg);

} // namespace watermark
