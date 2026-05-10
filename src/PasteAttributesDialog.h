#pragma once

#include <QDialog>
#include <QVector>
#include <QString>
#include "VideoEffect.h"
#include "EffectClipboard.h"

class QCheckBox;
class QDialogButtonBox;

class PasteAttributesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasteAttributesDialog(
        const QVector<VideoEffect>& clipboardEffects,
        const effectctrl::ClipMotion& clipboardMotion,
        QWidget *parent = nullptr);

    struct PasteSelection {
        bool pastePosition = false;
        bool pasteScale = false;
        bool pasteRotation = false;
        bool pasteOpacity = false;
        QVector<int> effectIndices;
    };

    PasteSelection selection() const;

private:
    QVector<QCheckBox*> m_effectChecks;
    QCheckBox *m_positionCheck = nullptr;
    QCheckBox *m_scaleCheck = nullptr;
    QCheckBox *m_rotationCheck = nullptr;
    QCheckBox *m_opacityCheck = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
};
