#include "AudioDuckingDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

// ---------------------------------------------------------------------------
// DuckingPreviewWidget — private helper, no Q_OBJECT needed
// ---------------------------------------------------------------------------
class DuckingPreviewWidget : public QWidget {
public:
    explicit DuckingPreviewWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(400, 200);
    }

    void setParams(const DuckingParams& p) {
        m_params = p;

        // Recompute DSP cache here (once per param change, not per paint).
        const int   sr          = 8000;
        const double durationSec = 4.0;
        const int   nSamples    = static_cast<int>(sr * durationSec);

        m_cachedSide.resize(nSamples);
        m_cachedSide.fill(0.0f);

        const double burst1Start = 0.5, burst1End = 1.5;
        const double burst2Start = 2.5, burst2End = 3.5;
        const double freq   = 440.0;
        const double twoPi  = 2.0 * M_PI;

        for (int i = 0; i < nSamples; ++i) {
            double t = static_cast<double>(i) / sr;
            float v = 0.0f;
            if (t >= burst1Start && t < burst1End) {
                double dt = t - burst1Start;
                v = static_cast<float>(std::sin(twoPi * freq * t) * std::exp(-dt * 2.0));
            } else if (t >= burst2Start && t < burst2End) {
                double dt = t - burst2Start;
                v = static_cast<float>(std::sin(twoPi * freq * t) * std::exp(-dt * 2.0));
            }
            m_cachedSide[i] = v;
        }

        m_cachedGain = computeDuckingGain(m_cachedSide, sr, m_params);
        m_minGain    = minGainLinear(m_params);

        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        // DSP data is pre-computed in setParams; paintEvent only draws.
        if (m_cachedSide.isEmpty() || m_cachedGain.isEmpty())
            return;

        const int nSamples = m_cachedSide.size();

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(30, 30, 30));

        const int W     = width();
        const int H     = height();
        const int halfH = H / 2;

        // Decimate to at most W points so the polyline stays cheap on resize.
        const int step = qMax(1, nSamples / qMax(W, 1));

        auto xOf = [&](int i) -> double {
            return static_cast<double>(i) / nSamples * W;
        };

        // --- Upper half: |sidechain| in green ---
        p.setPen(QPen(QColor(80, 180, 80), 1.5));
        {
            QPolygonF poly;
            poly.reserve(nSamples / step + 1);
            for (int i = 0; i < nSamples; i += step) {
                double amp = std::fabs(static_cast<double>(m_cachedSide[i]));
                double y   = (halfH - 2) - amp * (halfH - 4);
                poly.append(QPointF(xOf(i), y));
            }
            p.drawPolyline(poly);
        }

        // Divider
        p.setPen(QColor(80, 80, 80));
        p.drawLine(0, halfH, W, halfH);

        // --- Lower half: gain curve in orange ---
        p.setPen(QPen(QColor(220, 140, 40), 1.5));
        {
            QPolygonF poly;
            poly.reserve(nSamples / step + 1);
            const double range = 1.0 - m_minGain;
            for (int i = 0; i < nSamples; i += step) {
                double g    = static_cast<double>(m_cachedGain[i]);
                double norm = (range > 1e-9) ? (g - m_minGain) / range : 1.0;
                double y    = halfH + (halfH - 2) - norm * (halfH - 4);
                poly.append(QPointF(xOf(i), y));
            }
            p.drawPolyline(poly);
        }
    }

private:
    DuckingParams  m_params;
    QVector<float> m_cachedSide;
    QVector<float> m_cachedGain;
    double         m_minGain = 0.0;
};

// ---------------------------------------------------------------------------
// AudioDuckingDialog
// ---------------------------------------------------------------------------
AudioDuckingDialog::AudioDuckingDialog(const DuckingParams& initial, QWidget* parent)
    : QDialog(parent), m_params(initial)
{
    setWindowTitle(tr("Audio Ducking Settings"));

    // Widgets
    m_enabledCheck  = new QCheckBox(tr("Enable ducking"), this);
    m_enabledCheck->setChecked(true);

    auto makeSpin = [](double min, double max, double value, const QString& suffix) {
        auto* s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setSuffix(suffix);
        s->setValue(value);
        s->setDecimals(1);
        return s;
    };

    m_thresholdSpin = makeSpin(-60.0,    0.0, initial.thresholdDb,      QStringLiteral(" dB"));
    m_reductionSpin = makeSpin(-40.0,    0.0, initial.targetReductionDb, QStringLiteral(" dB"));
    m_attackSpin    = makeSpin(  1.0,  500.0, initial.attackMs,          QStringLiteral(" ms"));
    m_releaseSpin   = makeSpin( 10.0, 2000.0, initial.releaseMs,         QStringLiteral(" ms"));
    m_holdSpin      = makeSpin(  0.0, 1000.0, initial.holdMs,            QStringLiteral(" ms"));
    m_kneeSpin      = makeSpin(  0.0,   24.0, initial.kneeDb,            QStringLiteral(" dB"));

    m_preview = new DuckingPreviewWidget(this);

    // Layout
    auto* formLayout = new QFormLayout;
    formLayout->addRow(m_enabledCheck);
    formLayout->addRow(tr("Threshold:"),       m_thresholdSpin);
    formLayout->addRow(tr("Reduction:"),       m_reductionSpin);
    formLayout->addRow(tr("Attack:"),          m_attackSpin);
    formLayout->addRow(tr("Release:"),         m_releaseSpin);
    formLayout->addRow(tr("Hold:"),            m_holdSpin);
    formLayout->addRow(tr("Knee:"),            m_kneeSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_preview);
    mainLayout->addWidget(buttons);

    // Connections
    connect(m_thresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);
    connect(m_reductionSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);
    connect(m_attackSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);
    connect(m_releaseSpin,   QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);
    connect(m_holdSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);
    connect(m_kneeSpin,      QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AudioDuckingDialog::onParamsChanged);

    // Initial draw
    onParamsChanged();
}

void AudioDuckingDialog::onParamsChanged()
{
    m_params.thresholdDb      = m_thresholdSpin->value();
    m_params.targetReductionDb = m_reductionSpin->value();
    m_params.attackMs         = m_attackSpin->value();
    m_params.releaseMs        = m_releaseSpin->value();
    m_params.holdMs           = m_holdSpin->value();
    m_params.kneeDb           = m_kneeSpin->value();
    m_preview->setParams(m_params);
}

DuckingParams AudioDuckingDialog::params() const
{
    return m_params;
}

bool AudioDuckingDialog::duckingEnabled() const
{
    return m_enabledCheck->isChecked();
}
