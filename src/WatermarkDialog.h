#pragma once
#include <QDialog>
#include <QImage>
#include "WatermarkOverlay.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;

class WatermarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatermarkDialog(QWidget *parent = nullptr);

    watermark::WmConfig config() const { return m_cfg; }

private slots:
    void onModeChanged(int index);
    void onBrowseImageClicked();
    void onParamChanged();
    void onBatchApplyClicked();

private:
    void buildUi();
    void updatePreview();
    void syncModeWidgets();

    watermark::WmConfig m_cfg;
    QImage              m_sample;

    QComboBox    *m_modeCombo      = nullptr;
    QLineEdit    *m_imagePathEdit  = nullptr;
    QPushButton  *m_browseBtn      = nullptr;
    QLineEdit    *m_textEdit       = nullptr;
    QButtonGroup *m_posGroup       = nullptr;
    QSlider      *m_opacitySlider  = nullptr;
    QSlider      *m_scaleSlider    = nullptr;
    QSlider      *m_rotationSlider = nullptr;
    QLabel       *m_preview        = nullptr;
};
