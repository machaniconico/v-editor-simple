#pragma once

#include <QDockWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include "VideoEffect.h"
#include "LutImporter.h"

class ColorWheelWidget;

class ColorGradingPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit ColorGradingPanel(QWidget *parent = nullptr);

    void setColorCorrection(const ColorCorrection &cc);
    ColorCorrection colorCorrection() const { return m_cc; }

    void setLutList(const QVector<LutData> &luts);
    QString selectedLutName() const;
    double lutIntensity() const;

signals:
    void colorCorrectionChanged(const ColorCorrection &cc);
    void lutSelected(const QString &name);
    void lutIntensityChanged(double intensity);
    void resetRequested();

private slots:
    void onLiftChanged(double r, double g, double b);
    void onGammaWheelChanged(double r, double g, double b);
    void onGainChanged(double r, double g, double b);
    void onSliderChanged();
    void onLutComboChanged(int index);
    void onLutIntensityChanged(int value);
    void onResetClicked();

private:
    struct SliderRow {
        QSlider *slider;
        QLabel *valueLabel;
        QString paramName;
    };

    SliderRow addSlider(QLayout *layout, const QString &label,
                        int min, int max, int initial, int scale = 1);
    void updateSlidersFromCC();
    void blockSliderSignals(bool block);

    ColorCorrection m_cc;

    // Color wheels
    ColorWheelWidget *m_liftWheel;
    ColorWheelWidget *m_gammaWheel;
    ColorWheelWidget *m_gainWheel;

    // Basic correction sliders
    SliderRow m_exposure, m_brightness, m_contrast;
    SliderRow m_highlights, m_shadows;
    SliderRow m_saturation, m_hue;
    SliderRow m_temperature, m_tint;
    SliderRow m_gamma;

    // LUT controls
    QComboBox *m_lutCombo;
    QSlider *m_lutIntensitySlider;
    QLabel *m_lutIntensityLabel;

    QPushButton *m_resetButton;
    bool m_updating = false;
};
