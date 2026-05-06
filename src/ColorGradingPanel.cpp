#include "ColorGradingPanel.h"
#include "ColorWheelWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <cmath>

ColorGradingPanel::ColorGradingPanel(QWidget *parent)
    : QDockWidget(tr("カラーグレーディング"), parent)
{
    setObjectName("ColorGradingPanel");
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable
                | QDockWidget::DockWidgetFloatable);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget;
    auto *mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(8);

    // --- Color Wheels Section ---
    auto *wheelsGroup = new QGroupBox(tr("カラーホイール (Lift / Gamma / Gain)"));
    auto *wheelsLayout = new QHBoxLayout(wheelsGroup);
    wheelsLayout->setSpacing(4);

    m_liftWheel = new ColorWheelWidget(tr("Lift"));
    m_gammaWheel = new ColorWheelWidget(tr("Gamma"));
    m_gainWheel = new ColorWheelWidget(tr("Gain"));

    wheelsLayout->addWidget(m_liftWheel);
    wheelsLayout->addWidget(m_gammaWheel);
    wheelsLayout->addWidget(m_gainWheel);
    mainLayout->addWidget(wheelsGroup);

    connect(m_liftWheel, &ColorWheelWidget::colorChanged,
            this, &ColorGradingPanel::onLiftChanged);
    connect(m_gammaWheel, &ColorWheelWidget::colorChanged,
            this, &ColorGradingPanel::onGammaWheelChanged);
    connect(m_gainWheel, &ColorWheelWidget::colorChanged,
            this, &ColorGradingPanel::onGainChanged);

    // --- Basic Corrections Section ---
    auto *basicGroup = new QGroupBox(tr("基本補正"));
    auto *basicLayout = new QVBoxLayout(basicGroup);
    basicLayout->setSpacing(4);

    m_exposure   = addSlider(basicLayout, tr("露出"),         -300, 300, 0, 100);
    m_brightness = addSlider(basicLayout, tr("明るさ"),       -100, 100, 0);
    m_contrast   = addSlider(basicLayout, tr("コントラスト"), -100, 100, 0);
    m_highlights = addSlider(basicLayout, tr("ハイライト"),   -100, 100, 0);
    m_shadows    = addSlider(basicLayout, tr("シャドウ"),     -100, 100, 0);
    m_saturation = addSlider(basicLayout, tr("彩度"),         -100, 100, 0);
    m_hue        = addSlider(basicLayout, tr("色相"),         -180, 180, 0);
    m_temperature= addSlider(basicLayout, tr("色温度"),       -100, 100, 0);
    m_tint       = addSlider(basicLayout, tr("色かぶり"),     -100, 100, 0);
    m_gamma      = addSlider(basicLayout, tr("ガンマ"),        10, 300, 100, 100);

    mainLayout->addWidget(basicGroup);

    // --- LUT Section ---
    auto *lutGroup = new QGroupBox(tr("LUT"));
    auto *lutLayout = new QVBoxLayout(lutGroup);

    m_lutCombo = new QComboBox;
    m_lutCombo->addItem(tr("なし"));
    lutLayout->addWidget(m_lutCombo);

    auto *intensityRow = new QHBoxLayout;
    intensityRow->addWidget(new QLabel(tr("強度:")));
    m_lutIntensitySlider = new QSlider(Qt::Horizontal);
    m_lutIntensitySlider->setRange(0, 100);
    m_lutIntensitySlider->setValue(100);
    intensityRow->addWidget(m_lutIntensitySlider);
    m_lutIntensityLabel = new QLabel("100%");
    m_lutIntensityLabel->setMinimumWidth(40);
    intensityRow->addWidget(m_lutIntensityLabel);
    lutLayout->addLayout(intensityRow);

    mainLayout->addWidget(lutGroup);

    connect(m_lutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColorGradingPanel::onLutComboChanged);
    connect(m_lutIntensitySlider, &QSlider::valueChanged,
            this, &ColorGradingPanel::onLutIntensityChanged);

    // US-FEAT-C: Lift/Gamma/Gain wheels
    {
        // --- Lift Sliders ---
        auto *liftGroup = new QGroupBox(tr("Lift (シャドウ)"));
        m_liftSliders = addWheelSliders(liftGroup, LiftWheel);
        mainLayout->addWidget(liftGroup);

        // --- Gamma Sliders ---
        auto *gammaGroup = new QGroupBox(tr("Gamma (ミッドトーン)"));
        m_gammaSliders = addWheelSliders(gammaGroup, GammaWheel);
        mainLayout->addWidget(gammaGroup);

        // --- Gain Sliders ---
        auto *gainGroup = new QGroupBox(tr("Gain (ハイライト)"));
        m_gainSliders = addWheelSliders(gainGroup, GainWheel);
        mainLayout->addWidget(gainGroup);
    }

    // --- Debounce timer ---
    m_wheelDebounce = new QTimer(this);
    m_wheelDebounce->setSingleShot(true);
    m_wheelDebounce->setInterval(16);
    connect(m_wheelDebounce, &QTimer::timeout,
            this, &ColorGradingPanel::emitWheelsDebounced);

    // --- Reset Button ---
    m_resetButton = new QPushButton(tr("すべてリセット"));
    mainLayout->addWidget(m_resetButton);
    connect(m_resetButton, &QPushButton::clicked,
            this, &ColorGradingPanel::onResetClicked);

    mainLayout->addStretch();

    scrollArea->setWidget(content);
    setWidget(scrollArea);

    // Dark theme styling
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #444;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 16px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
        }
    )");
}

ColorGradingPanel::SliderRow ColorGradingPanel::addSlider(
    QLayout *layout, const QString &label, int min, int max, int initial, int scale)
{
    auto *row = new QHBoxLayout;
    auto *lbl = new QLabel(label);
    lbl->setMinimumWidth(70);
    row->addWidget(lbl);

    auto *slider = new QSlider(Qt::Horizontal);
    slider->setRange(min, max);
    slider->setValue(initial);
    row->addWidget(slider, 1);

    auto *valLabel = new QLabel;
    valLabel->setMinimumWidth(45);
    valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (scale > 1)
        valLabel->setText(QString::number(initial / static_cast<double>(scale), 'f', 2));
    else
        valLabel->setText(QString::number(initial));
    row->addWidget(valLabel);

    layout->addItem(row);

    SliderRow sr;
    sr.slider = slider;
    sr.valueLabel = valLabel;
    sr.paramName = label;

    connect(slider, &QSlider::valueChanged, this, &ColorGradingPanel::onSliderChanged);

    return sr;
}

ColorGradingPanel::WheelSliderGroup ColorGradingPanel::addWheelSliders(QGroupBox *group, ColorGradingPanel::WheelType type)
{
    auto *layout = new QVBoxLayout(group);
    layout->setSpacing(2);

    const bool isGamma = (type == GammaWheel);
    const int sliderMin = isGamma ? 0 : -100;
    const int sliderMax = isGamma ? 100 : 100;
    const int sliderInit = isGamma ? gammaToSlider(1.0) : 0;

    auto makeRow = [&](const QString &label) -> std::pair<QSlider*, QLabel*> {
        auto *row = new QHBoxLayout;
        auto *lbl = new QLabel(label);
        lbl->setMinimumWidth(30);
        row->addWidget(lbl);

        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(sliderMin, sliderMax);
        slider->setValue(sliderInit);
        row->addWidget(slider, 1);

        auto *val = new QLabel;
        val->setMinimumWidth(45);
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (isGamma)
            val->setText(QString::number(sliderToGamma(sliderInit), 'f', 2));
        else
            val->setText(QString::number(sliderToLiftGain(sliderInit), 'f', 2));
        row->addWidget(val);

        layout->addLayout(row);
        return {slider, val};
    };

    auto [rSlider, rLabel] = makeRow(tr("R:"));
    auto [gSlider, gLabel] = makeRow(tr("G:"));
    auto [bSlider, bLabel] = makeRow(tr("B:"));
    auto [lSlider, lLabel] = makeRow(tr("Luma:"));

    connect(rSlider, &QSlider::valueChanged, this, &ColorGradingPanel::onWheelSliderChanged);
    connect(gSlider, &QSlider::valueChanged, this, &ColorGradingPanel::onWheelSliderChanged);
    connect(bSlider, &QSlider::valueChanged, this, &ColorGradingPanel::onWheelSliderChanged);
    connect(lSlider, &QSlider::valueChanged, this, &ColorGradingPanel::onWheelSliderChanged);

    return {rSlider, gSlider, bSlider, lSlider, rLabel, gLabel, bLabel, lLabel};
}

double ColorGradingPanel::sliderToGamma(int v)
{
    double t = v / 100.0;
    return 0.1 * std::pow(40.0, t);
}

int ColorGradingPanel::gammaToSlider(double g)
{
    if (g <= 0.1) return 0;
    if (g >= 4.0) return 100;
    double t = std::log(g / 0.1) / std::log(40.0);
    return static_cast<int>(std::round(t * 100.0));
}

double ColorGradingPanel::sliderToLiftGain(int v)
{
    return v / 100.0;
}

int ColorGradingPanel::liftGainToSlider(double v)
{
    return static_cast<int>(std::round(v * 100.0));
}

void ColorGradingPanel::setColorCorrection(const ColorCorrection &cc)
{
    m_cc = cc;
    m_updating = true;
    m_liftWheel->setColor(cc.liftR, cc.liftG, cc.liftB);
    m_gammaWheel->setColor(cc.gammaR, cc.gammaG, cc.gammaB);
    m_gainWheel->setColor(cc.gainR, cc.gainG, cc.gainB);
    updateSlidersFromCC();
    m_updating = false;
}

void ColorGradingPanel::updateSlidersFromCC()
{
    blockSliderSignals(true);
    m_exposure.slider->setValue(static_cast<int>(m_cc.exposure * 100));
    m_brightness.slider->setValue(static_cast<int>(m_cc.brightness));
    m_contrast.slider->setValue(static_cast<int>(m_cc.contrast));
    m_highlights.slider->setValue(static_cast<int>(m_cc.highlights));
    m_shadows.slider->setValue(static_cast<int>(m_cc.shadows));
    m_saturation.slider->setValue(static_cast<int>(m_cc.saturation));
    m_hue.slider->setValue(static_cast<int>(m_cc.hue));
    m_temperature.slider->setValue(static_cast<int>(m_cc.temperature));
    m_tint.slider->setValue(static_cast<int>(m_cc.tint));
    m_gamma.slider->setValue(static_cast<int>(m_cc.gamma * 100));

    // Update value labels
    m_exposure.valueLabel->setText(QString::number(m_cc.exposure, 'f', 2));
    m_brightness.valueLabel->setText(QString::number(static_cast<int>(m_cc.brightness)));
    m_contrast.valueLabel->setText(QString::number(static_cast<int>(m_cc.contrast)));
    m_highlights.valueLabel->setText(QString::number(static_cast<int>(m_cc.highlights)));
    m_shadows.valueLabel->setText(QString::number(static_cast<int>(m_cc.shadows)));
    m_saturation.valueLabel->setText(QString::number(static_cast<int>(m_cc.saturation)));
    m_hue.valueLabel->setText(QString::number(static_cast<int>(m_cc.hue)));
    m_temperature.valueLabel->setText(QString::number(static_cast<int>(m_cc.temperature)));
    m_tint.valueLabel->setText(QString::number(static_cast<int>(m_cc.tint)));
    m_gamma.valueLabel->setText(QString::number(m_cc.gamma, 'f', 2));
    blockSliderSignals(false);
}

void ColorGradingPanel::blockSliderSignals(bool block)
{
    m_exposure.slider->blockSignals(block);
    m_brightness.slider->blockSignals(block);
    m_contrast.slider->blockSignals(block);
    m_highlights.slider->blockSignals(block);
    m_shadows.slider->blockSignals(block);
    m_saturation.slider->blockSignals(block);
    m_hue.slider->blockSignals(block);
    m_temperature.slider->blockSignals(block);
    m_tint.slider->blockSignals(block);
    m_gamma.slider->blockSignals(block);
}

void ColorGradingPanel::onLiftChanged(double r, double g, double b)
{
    if (m_updating) return;
    m_cc.liftR = r;
    m_cc.liftG = g;
    m_cc.liftB = b;
    emit colorCorrectionChanged(m_cc);
}

void ColorGradingPanel::onGammaWheelChanged(double r, double g, double b)
{
    if (m_updating) return;
    m_cc.gammaR = r;
    m_cc.gammaG = g;
    m_cc.gammaB = b;
    emit colorCorrectionChanged(m_cc);
}

void ColorGradingPanel::onGainChanged(double r, double g, double b)
{
    if (m_updating) return;
    m_cc.gainR = r;
    m_cc.gainG = g;
    m_cc.gainB = b;
    emit colorCorrectionChanged(m_cc);
}

void ColorGradingPanel::onSliderChanged()
{
    if (m_updating) return;

    m_cc.exposure    = m_exposure.slider->value() / 100.0;
    m_cc.brightness  = m_brightness.slider->value();
    m_cc.contrast    = m_contrast.slider->value();
    m_cc.highlights  = m_highlights.slider->value();
    m_cc.shadows     = m_shadows.slider->value();
    m_cc.saturation  = m_saturation.slider->value();
    m_cc.hue         = m_hue.slider->value();
    m_cc.temperature = m_temperature.slider->value();
    m_cc.tint        = m_tint.slider->value();
    m_cc.gamma       = m_gamma.slider->value() / 100.0;

    // Update labels
    m_exposure.valueLabel->setText(QString::number(m_cc.exposure, 'f', 2));
    m_brightness.valueLabel->setText(QString::number(static_cast<int>(m_cc.brightness)));
    m_contrast.valueLabel->setText(QString::number(static_cast<int>(m_cc.contrast)));
    m_highlights.valueLabel->setText(QString::number(static_cast<int>(m_cc.highlights)));
    m_shadows.valueLabel->setText(QString::number(static_cast<int>(m_cc.shadows)));
    m_saturation.valueLabel->setText(QString::number(static_cast<int>(m_cc.saturation)));
    m_hue.valueLabel->setText(QString::number(static_cast<int>(m_cc.hue)));
    m_temperature.valueLabel->setText(QString::number(static_cast<int>(m_cc.temperature)));
    m_tint.valueLabel->setText(QString::number(static_cast<int>(m_cc.tint)));
    m_gamma.valueLabel->setText(QString::number(m_cc.gamma, 'f', 2));

    emit colorCorrectionChanged(m_cc);
}

void ColorGradingPanel::onWheelSliderChanged()
{
    if (m_updating) return;

    auto readLiftGainSliders = [](const WheelSliderGroup &g) -> std::pair<QVector3D, double> {
        return {
            QVector3D(sliderToLiftGain(g.r->value()),
                       sliderToLiftGain(g.g->value()),
                       sliderToLiftGain(g.b->value())),
            sliderToLiftGain(g.luma->value())
        };
    };

    auto readGammaSliders = [](const WheelSliderGroup &g) -> std::pair<QVector3D, double> {
        return {
            QVector3D(static_cast<float>(sliderToGamma(g.r->value())),
                       static_cast<float>(sliderToGamma(g.g->value())),
                       static_cast<float>(sliderToGamma(g.b->value()))),
            sliderToGamma(g.luma->value())
        };
    };

    auto [liftVec, liftLuma] = readLiftGainSliders(m_liftSliders);
    auto [gammaVec, gammaLuma] = readGammaSliders(m_gammaSliders);
    auto [gainVec, gainLuma] = readLiftGainSliders(m_gainSliders);

    m_wheels.lift = liftVec;
    m_wheels.liftLuma = liftLuma;
    m_wheels.gamma = gammaVec;
    m_wheels.gammaLuma = gammaLuma;
    m_wheels.gain = gainVec;
    m_wheels.gainLuma = gainLuma;

    // Update labels
    auto fmtLiftGain = [](double v) { return QString::number(v, 'f', 2); };
    auto fmtGamma = [](double v) { return QString::number(v, 'f', 2); };

    m_liftSliders.rLabel->setText(fmtLiftGain(sliderToLiftGain(m_liftSliders.r->value())));
    m_liftSliders.gLabel->setText(fmtLiftGain(sliderToLiftGain(m_liftSliders.g->value())));
    m_liftSliders.bLabel->setText(fmtLiftGain(sliderToLiftGain(m_liftSliders.b->value())));
    m_liftSliders.lumaLabel->setText(fmtLiftGain(liftLuma));

    m_gammaSliders.rLabel->setText(fmtGamma(sliderToGamma(m_gammaSliders.r->value())));
    m_gammaSliders.gLabel->setText(fmtGamma(sliderToGamma(m_gammaSliders.g->value())));
    m_gammaSliders.bLabel->setText(fmtGamma(sliderToGamma(m_gammaSliders.b->value())));
    m_gammaSliders.lumaLabel->setText(fmtGamma(gammaLuma));

    m_gainSliders.rLabel->setText(fmtLiftGain(sliderToLiftGain(m_gainSliders.r->value())));
    m_gainSliders.gLabel->setText(fmtLiftGain(sliderToLiftGain(m_gainSliders.g->value())));
    m_gainSliders.bLabel->setText(fmtLiftGain(sliderToLiftGain(m_gainSliders.b->value())));
    m_gainSliders.lumaLabel->setText(fmtLiftGain(gainLuma));

    m_wheelDebounce->start();
}

void ColorGradingPanel::emitWheelsDebounced()
{
    emit colorWheelsChanged(m_wheels);
}

ColorWheels ColorGradingPanel::currentWheels() const
{
    return m_wheels;
}

void ColorGradingPanel::setWheels(const ColorWheels &cw)
{
    m_wheels = cw;

    QSignalBlocker b1(m_liftSliders.r);
    QSignalBlocker b2(m_liftSliders.g);
    QSignalBlocker b3(m_liftSliders.b);
    QSignalBlocker b4(m_liftSliders.luma);
    QSignalBlocker b5(m_gammaSliders.r);
    QSignalBlocker b6(m_gammaSliders.g);
    QSignalBlocker b7(m_gammaSliders.b);
    QSignalBlocker b8(m_gammaSliders.luma);
    QSignalBlocker b9(m_gainSliders.r);
    QSignalBlocker ba(m_gainSliders.g);
    QSignalBlocker bb(m_gainSliders.b);
    QSignalBlocker bc(m_gainSliders.luma);

    m_liftSliders.r->setValue(liftGainToSlider(cw.lift.x()));
    m_liftSliders.g->setValue(liftGainToSlider(cw.lift.y()));
    m_liftSliders.b->setValue(liftGainToSlider(cw.lift.z()));
    m_liftSliders.luma->setValue(liftGainToSlider(cw.liftLuma));

    m_gammaSliders.r->setValue(gammaToSlider(static_cast<double>(cw.gamma.x())));
    m_gammaSliders.g->setValue(gammaToSlider(static_cast<double>(cw.gamma.y())));
    m_gammaSliders.b->setValue(gammaToSlider(static_cast<double>(cw.gamma.z())));
    m_gammaSliders.luma->setValue(gammaToSlider(cw.gammaLuma));

    m_gainSliders.r->setValue(liftGainToSlider(cw.gain.x()));
    m_gainSliders.g->setValue(liftGainToSlider(cw.gain.y()));
    m_gainSliders.b->setValue(liftGainToSlider(cw.gain.z()));
    m_gainSliders.luma->setValue(liftGainToSlider(cw.gainLuma));

    // Update labels
    m_liftSliders.rLabel->setText(QString::number(static_cast<double>(cw.lift.x()), 'f', 2));
    m_liftSliders.gLabel->setText(QString::number(static_cast<double>(cw.lift.y()), 'f', 2));
    m_liftSliders.bLabel->setText(QString::number(static_cast<double>(cw.lift.z()), 'f', 2));
    m_liftSliders.lumaLabel->setText(QString::number(cw.liftLuma, 'f', 2));

    m_gammaSliders.rLabel->setText(QString::number(static_cast<double>(cw.gamma.x()), 'f', 2));
    m_gammaSliders.gLabel->setText(QString::number(static_cast<double>(cw.gamma.y()), 'f', 2));
    m_gammaSliders.bLabel->setText(QString::number(static_cast<double>(cw.gamma.z()), 'f', 2));
    m_gammaSliders.lumaLabel->setText(QString::number(cw.gammaLuma, 'f', 2));

    m_gainSliders.rLabel->setText(QString::number(static_cast<double>(cw.gain.x()), 'f', 2));
    m_gainSliders.gLabel->setText(QString::number(static_cast<double>(cw.gain.y()), 'f', 2));
    m_gainSliders.bLabel->setText(QString::number(static_cast<double>(cw.gain.z()), 'f', 2));
    m_gainSliders.lumaLabel->setText(QString::number(cw.gainLuma, 'f', 2));
}

void ColorGradingPanel::setLutList(const QVector<LutData> &luts)
{
    m_lutCombo->blockSignals(true);
    m_lutCombo->clear();
    m_lutCombo->addItem(tr("なし"));
    for (const auto &lut : luts)
        m_lutCombo->addItem(lut.name);
    m_lutCombo->blockSignals(false);
}

QString ColorGradingPanel::selectedLutName() const
{
    if (m_lutCombo->currentIndex() <= 0)
        return QString();
    return m_lutCombo->currentText();
}

double ColorGradingPanel::lutIntensity() const
{
    return m_lutIntensitySlider->value() / 100.0;
}

void ColorGradingPanel::onLutComboChanged(int index)
{
    if (index <= 0)
        emit lutSelected(QString());
    else
        emit lutSelected(m_lutCombo->currentText());
}

void ColorGradingPanel::onLutIntensityChanged(int value)
{
    m_lutIntensityLabel->setText(QString("%1%").arg(value));
    emit lutIntensityChanged(value / 100.0);
}

void ColorGradingPanel::onResetClicked()
{
    m_cc.reset();
    m_updating = true;

    m_liftWheel->setColor(0, 0, 0);
    m_gammaWheel->setColor(0, 0, 0);
    m_gainWheel->setColor(0, 0, 0);
    updateSlidersFromCC();

    // US-FEAT-C: reset wheel sliders to neutral
    ColorWheels neutral;
    setWheels(neutral);

    m_updating = false;

    m_lutCombo->setCurrentIndex(0);
    m_lutIntensitySlider->setValue(100);

    emit colorCorrectionChanged(m_cc);
    emit colorWheelsChanged(m_wheels);
    emit resetRequested();
}
