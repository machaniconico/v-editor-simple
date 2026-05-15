#pragma once

#include <QObject>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>

// ---------------------------------------------------------------------------
// Sprint 22 / US-GIF-1 — Animated export dialog (modeless)
//
// Picks an input folder of sequentially-named images, lets the user choose
// GIF/WebP, fps and width, then exports via animexport::exportFrames.
// ---------------------------------------------------------------------------

class AnimatedExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AnimatedExportDialog(QWidget *parent = nullptr);

private slots:
    void onBrowseFolderClicked();
    void onExportClicked();

private:
    QLineEdit   *m_folderEdit  = nullptr;
    QComboBox   *m_formatCombo = nullptr;
    QSpinBox    *m_fpsSpin     = nullptr;
    QSpinBox    *m_widthSpin   = nullptr;
    QPushButton *m_exportBtn   = nullptr;
};
