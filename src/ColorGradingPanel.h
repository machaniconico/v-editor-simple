#pragma once

#include <QDockWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <QVector3D>
#include "VideoEffect.h"
#include "LutImporter.h"

class ColorWheelWidget;
class QGroupBox;
class QLayout;

// US-FEAT-C: Lift/Gamma/Gain wheels
struct ColorWheels {
    QVector3D lift   = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D gamma  = QVector3D(1.0f, 1.0f, 1.0f);
    QVector3D gain   = QVector3D(0.0f, 0.0f, 0.0f);
    double liftLuma  = 0.0;
    double gammaLuma = 1.0;
    double gainLuma  = 0.0;

    bool operator==(const ColorWheels &o) const {
        return lift == o.lift && gamma == o.gamma && gain == o.gain
            && liftLuma == o.liftLuma && gammaLuma == o.gammaLuma && gainLuma == o.gainLuma;
    }
    bool operator!=(const ColorWheels &o) const { return !(*this == o); }
};

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

    ColorWheels currentWheels() const;
    void setWheels(const ColorWheels &cw);

signals:
    void colorCorrectionChanged(const ColorCorrection &cc);
    void colorWheelsChanged(const ColorWheels &cw);
    void lutSelected(const QString &name);
    void lutIntensityChanged(double intensity);
    void resetRequested();

private slots:
    void onLiftChanged(double r, double g, double b);
    void onGammaWheelChanged(double r, double g, double b);
    void onGainChanged(double r, double g, double b);
    void onSliderChanged();
    void onWheelSliderChanged();
    void emitWheelsDebounced();
    void onLutComboChanged(int index);
    void onLutIntensityChanged(int value);
    void onResetClicked();

private:
    struct SliderRow {
        QSlider *slider;
        QLabel *valueLabel;
        QString paramName;
    };

    struct WheelSliderGroup {
        QSlider *r, *g, *b, *luma;
        QLabel *rLabel, *gLabel, *bLabel, *lumaLabel;
    };

    SliderRow addSlider(QLayout *layout, const QString &label,
                        int min, int max, int initial, int scale = 1);
    enum WheelType { LiftWheel, GammaWheel, GainWheel };
    WheelSliderGroup addWheelSliders(QGroupBox *group, WheelType type);
    void updateSlidersFromCC();
    void blockSliderSignals(bool block);
    static double sliderToGamma(int v);
    static int gammaToSlider(double g);
    static double sliderToLiftGain(int v);
    static int liftGainToSlider(double v);

    ColorCorrection m_cc;
    ColorWheels m_wheels;

    // Color wheels (graphical)
    ColorWheelWidget *m_liftWheel;
    ColorWheelWidget *m_gammaWheel;
    ColorWheelWidget *m_gainWheel;

    // Lift/Gamma/Gain slider groups
    WheelSliderGroup m_liftSliders;
    WheelSliderGroup m_gammaSliders;
    WheelSliderGroup m_gainSliders;

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
    QTimer *m_wheelDebounce;
    bool m_updating = false;
};
