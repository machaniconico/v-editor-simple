#include "EffectClipboard.h"

namespace effectctrl {

EffectClipboard& EffectClipboard::instance()
{
    static EffectClipboard s_instance;
    return s_instance;
}

void EffectClipboard::capture(const QVector<VideoEffect>& effects, const ClipMotion& motion)
{
    m_effectsClipboard = effects;
    m_motionClipboard = motion;
    emit contentChanged();
}

QVector<VideoEffect> EffectClipboard::effects() const
{
    return m_effectsClipboard;
}

ClipMotion EffectClipboard::motion() const
{
    return m_motionClipboard;
}

bool EffectClipboard::hasContent() const
{
    return !m_effectsClipboard.isEmpty();
}

} // namespace effectctrl
