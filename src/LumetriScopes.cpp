#include "LumetriScopes.h"

#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QtMath>

namespace {

// Decimate the source frame to a fixed pixel budget so scope math stays
// O(constant) regardless of preview size. ~10k samples produces stable
// histograms at <1 ms cost on a single thread.
constexpr int kSampleBudget = 10000;

inline QImage downsampleToBudget(const QImage &src) {
    const int srcArea = src.width() * src.height();
    if (srcArea <= kSampleBudget) return src;
    const double scale = std::sqrt(static_cast<double>(kSampleBudget) /
                                   static_cast<double>(srcArea));
    const int w = qMax(16, static_cast<int>(src.width()  * scale));
    const int h = qMax(16, static_cast<int>(src.height() * scale));
    return src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
              .convertToFormat(QImage::Format_RGB32);
}

// Vectorscope-specific downsample: cap at 64x64 grid (= 4096 samples) for
// bounded CPU. The polar accumulation runs on every repaint but costs O(n)
// in decoded pixels; keeping n ≤ 4096 keeps < 1 ms on a single thread.
inline QImage downsampleForVector(const QImage &src) {
    constexpr int kMaxGrid = 64 * 64;
    const int srcArea = src.width() * src.height();
    if (srcArea <= kMaxGrid)
        return src.convertToFormat(QImage::Format_RGB32);
    const double scale = std::sqrt(static_cast<double>(kMaxGrid) /
                                   static_cast<double>(srcArea));
    const int w = qMax(4, static_cast<int>(src.width()  * scale));
    const int h = qMax(4, static_cast<int>(src.height() * scale));
    return src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
              .convertToFormat(QImage::Format_RGB32);
}

} // namespace

// ----- Histogram -----------------------------------------------------------

class HistogramWidget : public QWidget
{
public:
    explicit HistogramWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(96);
    }
    void setFrame(const QImage &frame) {
        const QImage src = downsampleToBudget(frame).convertToFormat(QImage::Format_RGB32);
        std::fill(std::begin(m_r), std::end(m_r), 0);
        std::fill(std::begin(m_g), std::end(m_g), 0);
        std::fill(std::begin(m_b), std::end(m_b), 0);
        for (int y = 0; y < src.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
            for (int x = 0; x < src.width(); ++x) {
                const QRgb px = line[x];
                ++m_r[qRed(px)];
                ++m_g[qGreen(px)];
                ++m_b[qBlue(px)];
            }
        }
        m_peak = 0;
        for (int i = 0; i < 256; ++i)
            m_peak = qMax(m_peak, qMax(m_r[i], qMax(m_g[i], m_b[i])));
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(20, 20, 22));
        if (m_peak <= 0) return;
        const double xs = static_cast<double>(width()) / 256.0;
        const double ys = static_cast<double>(height()) / static_cast<double>(m_peak);
        auto draw = [&](const int *bins, QColor c) {
            c.setAlpha(170);
            p.setPen(QPen(c, 1));
            for (int i = 0; i < 256; ++i) {
                const int h = static_cast<int>(bins[i] * ys);
                const int x = static_cast<int>(i * xs);
                p.drawLine(x, height(), x, height() - h);
            }
        };
        draw(m_r, QColor(230,  70,  70));
        draw(m_g, QColor( 80, 220,  90));
        draw(m_b, QColor( 90, 130, 240));
        p.setPen(QColor(180, 180, 180));
        p.drawText(4, 12, "Histogram");
    }
private:
    int m_r[256] = {};
    int m_g[256] = {};
    int m_b[256] = {};
    int m_peak = 0;
};

// ----- Luma Waveform -------------------------------------------------------

class WaveformWidget : public QWidget
{
public:
    explicit WaveformWidget(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(120);
    }
    void setFrame(const QImage &frame) {
        m_src = downsampleToBudget(frame).convertToFormat(QImage::Format_RGB32);
        update();
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor(20, 20, 22));
        if (m_src.isNull()) return;
        // X axis = source column (mapped to widget width). Y axis = luma
        // 0 (bottom) .. 255 (top). Each pixel of the downsampled source
        // contributes one point.
        const int W = width();
        const int H = height();
        if (W <= 0 || H <= 0) return;
        const double xs = static_cast<double>(W) / static_cast<double>(m_src.width());
        const double ys = static_cast<double>(H) / 255.0;
        p.setPen(QPen(QColor(220, 220, 220, 150), 1));
        for (int y = 0; y < m_src.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(m_src.constScanLine(y));
            for (int x = 0; x < m_src.width(); ++x) {
                const QRgb px = line[x];
                const int luma = qRound(0.299 * qRed(px)
                                        + 0.587 * qGreen(px)
                                        + 0.114 * qBlue(px));
                const int sx = static_cast<int>(x * xs);
                const int sy = H - 1 - static_cast<int>(luma * ys);
                p.drawPoint(sx, sy);
            }
        }
        // Reference lines at 0 / 50 / 100 IRE.
        p.setPen(QPen(QColor(120, 120, 120, 120), 1, Qt::DashLine));
        p.drawLine(0, H - 1, W, H - 1);
        p.drawLine(0, H - 1 - static_cast<int>(128 * ys), W, H - 1 - static_cast<int>(128 * ys));
        p.drawLine(0, 0, W, 0);
        p.setPen(QColor(180, 180, 180));
        p.drawText(4, 12, "Luma Waveform");
    }
private:
    QImage m_src;
};

// ----- Vectorscope ---------------------------------------------------------

class VectorscopeWidget : public QWidget
{
public:
    explicit VectorscopeWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(160);
        m_densityBuf = QImage(256, 256, QImage::Format_ARGB32_Premultiplied);
    }

    void setFrame(const QImage &frame)
    {
        m_src = downsampleForVector(frame);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(20, 20, 22));
        if (m_src.isNull()) return;

        const int W = width();
        const int H = height();
        const int sz = qMin(W, H);
        const int cx = W / 2;
        const int cy = H / 2;
        const int radius = sz / 2 - 16;

        // --- 1. Build density accumulation image (256×256) ---------------
        constexpr int kDensSz = 256;
        constexpr int kAlpha = 60;

        m_densityBuf.fill(Qt::transparent);
        QPainter dp(&m_densityBuf);
        dp.setRenderHint(QPainter::Antialiasing, false);
        dp.setPen(Qt::NoPen);
        dp.setBrush(QColor(255, 255, 255, kAlpha));

        // Map [Cb/Cr - 128] ∈ [-128, 128]  →  [0, kDensSz)
        const double mapS = static_cast<double>(kDensSz) / 256.0;

        for (int y = 0; y < m_src.height(); ++y) {
            const QRgb *line =
                reinterpret_cast<const QRgb *>(m_src.constScanLine(y));
            for (int x = 0; x < m_src.width(); ++x) {
                const QRgb px = line[x];
                float fy, fcb, fcr;
                LumetriColor::rgbToYCbCr709(qRed(px), qGreen(px),
                                            qBlue(px), fy, fcb, fcr);

                // Cb → x (horizontal); Cr → y (inverted for screen-up)
                const double nx = (fcb - 128.0) * mapS + kDensSz / 2.0;
                const double ny = -(fcr - 128.0) * mapS + kDensSz / 2.0;

                dp.drawEllipse(QPointF(nx, ny), 2.0, 2.0);
            }
        }
        dp.end();

        // --- 2. Blit density image to widget centre -----------------------
        const QRect destRect(cx - radius, cy - radius, radius * 2, radius * 2);
        p.drawImage(destRect, m_densityBuf);

        // --- 3. Graticule: 75 %-saturation hexagon + target boxes ---------
        struct Target {
            const char *label;
            int r, g, b;  // 75 % colour-bar values
        };
        static const Target kTargets[] = {
            {"R",  191, 0,   0  },
            {"Yl", 191, 191, 0  },
            {"G",  0,   191, 0  },
            {"Cy", 0,   191, 191},
            {"B",  0,   0,   191},
            {"Mg", 191, 0,   191},
        };

        QPointF targetPts[6];
        for (int i = 0; i < 6; ++i) {
            float fy, fcb, fcr;
            LumetriColor::rgbToYCbCr709(kTargets[i].r, kTargets[i].g,
                                        kTargets[i].b, fy, fcb, fcr);
            const double tx = cx + (fcb - 128.0) / 128.0 * radius;
            const double ty = cy - (fcr - 128.0) / 128.0 * radius;
            targetPts[i] = QPointF(tx, ty);
        }

        // Hexagon outline
        p.setPen(QPen(QColor(120, 120, 120, 100), 1, Qt::DashLine));
        for (int i = 0; i < 6; ++i)
            p.drawLine(targetPts[i], targetPts[(i + 1) % 6]);

        // Target boxes
        constexpr int kBox = 7;
        p.setPen(QPen(QColor(200, 200, 200, 160), 1));
        for (int i = 0; i < 6; ++i) {
            p.drawRect(QRectF(targetPts[i].x() - kBox / 2.0,
                              targetPts[i].y() - kBox / 2.0, kBox, kBox));
            // Label
            p.drawText(QPointF(targetPts[i].x() + 5, targetPts[i].y() + 4),
                       QString::fromLatin1(kTargets[i].label));
        }

        // --- 4. Skin-tone line at +33° from +Cb axis ---------------------
        constexpr double kSkinDeg = 33.0;
        const double skinRad = kSkinDeg * M_PI / 180.0;
        const double skDx = std::cos(skinRad) * radius;
        const double skDy = -std::sin(skinRad) * radius;  // flip for screen-Y
        p.setPen(QPen(QColor(220, 180, 140, 130), 1, Qt::DotLine));
        p.drawLine(QPointF(cx, cy), QPointF(cx + skDx, cy + skDy));

        // --- 5. Crosshair reference ---------------------------------------
        p.setPen(QPen(QColor(120, 120, 120, 100), 1, Qt::DashLine));
        p.drawLine(QPointF(cx - radius, cy), QPointF(cx + radius, cy));
        p.drawLine(QPointF(cx, cy - radius), QPointF(cx, cy + radius));

        // --- 6. Label -----------------------------------------------------
        p.setPen(QColor(180, 180, 180));
        p.drawText(4, 12, "Vectorscope");
    }

private:
    QImage m_src;
    QImage m_densityBuf;
};

// ----- LumetriScopes container --------------------------------------------

LumetriScopes::LumetriScopes(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);
    m_hist = new HistogramWidget(this);
    m_wave = new WaveformWidget(this);
    m_vector = new VectorscopeWidget(this);
    layout->addWidget(m_hist);
    layout->addWidget(m_wave);

    // Toggle row: checkbox + stretch
    auto *toggleRow = new QHBoxLayout();
    toggleRow->setContentsMargins(4, 0, 4, 0);
    m_vecToggle = new QCheckBox(QString::fromUtf8("Vectorscope"), this);
    m_vecToggle->setChecked(m_showVectorscope);
    toggleRow->addWidget(m_vecToggle);
    toggleRow->addStretch();
    layout->addLayout(toggleRow);

    layout->addWidget(m_vector, 1);
    m_vector->setVisible(m_showVectorscope);

    connect(m_vecToggle, &QCheckBox::toggled, this, [this](bool checked) {
        m_showVectorscope = checked;
        m_vector->setVisible(checked);
    });

    m_throttle.start();
}

void LumetriScopes::setFrame(const QImage &frame)
{
    if (frame.isNull()) return;
    if (!isVisible()) return;
    if (m_throttle.elapsed() < kThrottleMs) return;
    m_throttle.restart();
    if (m_hist) m_hist->setFrame(frame);
    if (m_wave) m_wave->setFrame(frame);
    if (m_vector && m_showVectorscope) m_vector->setFrame(frame);
}
