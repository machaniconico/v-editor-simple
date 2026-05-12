#pragma once

#include "Text3DLayer.h"

#include <QColor>
#include <QDialog>
#include <QFont>
#include <QString>
#include <QVector3D>

class QDialogButtonBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

class Text3DExtrusionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit Text3DExtrusionDialog(QWidget *parent = nullptr);

    // Copy layer state into the widgets.
    void setLayer(const Text3DLayer &layer);

    // Return a heap-allocated Text3DLayer configured from the current widget
    // state with extrude ENABLED.  Text3DLayer inherits QObject and is
    // therefore non-copyable; the caller takes ownership (parent it or delete
    // it).  Pass nullptr as parent to get an unparented object.
    Text3DLayer *layer(QObject *layerParent = nullptr) const;

signals:
    void layerChanged();

private slots:
    void onTextChanged();
    void onFontButtonClicked();
    void onExtrudeParamChanged();
    void onFrontColorClicked();
    void onSideColorClicked();
    void onAmbientColorClicked();
    void onOrientationChanged();
    void onPreviewSliderChanged();
    void updatePreview();

private:
    void buildLayer(Text3DLayer &out) const;

    // --- Text / font ---
    QLineEdit       *m_textEdit       = nullptr;
    QPushButton     *m_fontButton     = nullptr;
    QFont            m_currentFont;

    // --- Extrude params ---
    QGroupBox       *m_extrudeBox     = nullptr;
    QDoubleSpinBox  *m_depthSpin      = nullptr;
    QDoubleSpinBox  *m_bevelDepthSpin = nullptr;
    QDoubleSpinBox  *m_bevelWidthSpin = nullptr;
    QSpinBox        *m_bevelSegSpin   = nullptr;

    // --- Material ---
    QGroupBox       *m_materialBox    = nullptr;
    QPushButton     *m_frontColorBtn  = nullptr;
    QPushButton     *m_sideColorBtn   = nullptr;
    QPushButton     *m_ambientBtn     = nullptr;
    QColor           m_frontColor;
    QColor           m_sideColor;
    QColor           m_ambientColor;
    QDoubleSpinBox  *m_lightXSpin     = nullptr;
    QDoubleSpinBox  *m_lightYSpin     = nullptr;
    QDoubleSpinBox  *m_lightZSpin     = nullptr;

    // --- Orientation / spin ---
    QGroupBox       *m_orientBox      = nullptr;
    QDoubleSpinBox  *m_yawSpin        = nullptr;
    QDoubleSpinBox  *m_pitchSpin      = nullptr;
    QDoubleSpinBox  *m_camDistSpin    = nullptr;
    QDoubleSpinBox  *m_spinSpeedSpin  = nullptr;

    // --- Preview ---
    QLabel          *m_previewLabel   = nullptr;
    QSlider         *m_timeSider      = nullptr;

    QDialogButtonBox *m_buttonBox     = nullptr;
};
