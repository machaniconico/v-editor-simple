#include "ColorGradingPanel.h"
#include "ColorWheelWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QScrollArea>

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
    m_updating = false;

    m_lutCombo->setCurrentIndex(0);
    m_lutIntensitySlider->setValue(100);

    emit colorCorrectionChanged(m_cc);
    emit resetRequested();
}
