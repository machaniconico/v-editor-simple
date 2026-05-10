#include "PasteAttributesDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QCheckBox>

PasteAttributesDialog::PasteAttributesDialog(
    const QVector<VideoEffect>& clipboardEffects,
    const effectctrl::ClipMotion& /*clipboardMotion*/,
    QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Paste Attributes"));
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);

    auto *motionGroup = new QGroupBox(tr("Motion"), this);
    auto *motionLayout = new QVBoxLayout(motionGroup);

    m_positionCheck = new QCheckBox(tr("Position"), motionGroup);
    m_positionCheck->setChecked(true);
    motionLayout->addWidget(m_positionCheck);

    m_scaleCheck = new QCheckBox(tr("Scale"), motionGroup);
    m_scaleCheck->setChecked(true);
    motionLayout->addWidget(m_scaleCheck);

    m_rotationCheck = new QCheckBox(tr("Rotation"), motionGroup);
    m_rotationCheck->setChecked(true);
    motionLayout->addWidget(m_rotationCheck);

    m_opacityCheck = new QCheckBox(tr("Opacity"), motionGroup);
    m_opacityCheck->setChecked(true);
    motionLayout->addWidget(m_opacityCheck);

    mainLayout->addWidget(motionGroup);

    if (!clipboardEffects.isEmpty()) {
        auto *fxGroup = new QGroupBox(tr("Effects"), this);
        auto *fxLayout = new QVBoxLayout(fxGroup);

        for (int i = 0; i < clipboardEffects.size(); ++i) {
            auto *cb = new QCheckBox(VideoEffect::typeName(clipboardEffects[i].type), fxGroup);
            cb->setChecked(true);
            m_effectChecks.append(cb);
            fxLayout->addWidget(cb);
        }

        mainLayout->addWidget(fxGroup);
    }

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);
}

PasteAttributesDialog::PasteSelection PasteAttributesDialog::selection() const
{
    PasteSelection sel;
    sel.pastePosition = m_positionCheck && m_positionCheck->isChecked();
    sel.pasteScale = m_scaleCheck && m_scaleCheck->isChecked();
    sel.pasteRotation = m_rotationCheck && m_rotationCheck->isChecked();
    sel.pasteOpacity = m_opacityCheck && m_opacityCheck->isChecked();

    for (int i = 0; i < m_effectChecks.size(); ++i) {
        if (m_effectChecks[i]->isChecked())
            sel.effectIndices.append(i);
    }

    return sel;
}
