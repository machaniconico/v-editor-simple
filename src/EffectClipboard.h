#pragma once

#include <QObject>
#include <QVector>
#include "VideoEffect.h"

namespace effectctrl {

struct ClipMotion {
    double scale = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    double rotationDeg = 0.0;
    double opacity = 1.0;
};

class EffectClipboard : public QObject
{
    Q_OBJECT

public:
    static EffectClipboard& instance();

    void capture(const QVector<VideoEffect>& effects, const ClipMotion& motion);
    QVector<VideoEffect> effects() const;
    ClipMotion motion() const;
    bool hasContent() const;

signals:
    void contentChanged();

private:
    EffectClipboard() = default;

    QVector<VideoEffect> m_effectsClipboard;
    ClipMotion m_motionClipboard;
};

} // namespace effectctrl
