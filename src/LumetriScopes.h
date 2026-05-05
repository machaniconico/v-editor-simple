#pragma once

#include <QImage>
#include <QWidget>
#include <QElapsedTimer>
#include <QtMath>

class QCheckBox;
class HistogramWidget;
class WaveformWidget;
class VectorscopeWidget;

// BT.709 Y'CbCr conversion helpers for vectorscope rendering.
namespace LumetriColor {
inline void rgbToYCbCr709(int r, int g, int b, float &y, float &cb, float &cr)
{
    y  = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    cb = -0.1146f * r - 0.3854f * g + 0.5000f * b + 128.0f;
    cr =  0.5000f * r - 0.4542f * g - 0.0458f * b + 128.0f;
}

inline void rgbToYCbCr709(int r, int g, int b, int &y, int &cb, int &cr)
{
    float fy, fcb, fcr;
    rgbToYCbCr709(r, g, b, fy, fcb, fcr);
    y  = qBound(0, qRound(fy),  255);
    cb = qBound(0, qRound(fcb), 255);
    cr = qBound(0, qRound(fcr), 255);
}
} // namespace LumetriColor

// Lumetri-style measurement scopes (RGB Histogram + Luma Waveform +
// Vectorscope). Three internal sub-widgets stacked vertically inside one
// container. The container exposes a single setFrame() slot that updates
// all three at once. Frame ingestion is throttled to ~10 fps so a 60 fps
// preview doesn't burn the GUI thread on histogram math.
class LumetriScopes : public QWidget
{
    Q_OBJECT
public:
    explicit LumetriScopes(QWidget *parent = nullptr);

public slots:
    // Drop-in target for VideoPlayer::frameComposited. Internally rate-
    // limits to roughly the kThrottleMs interval — for cheap shipboard
    // monitoring rather than precision colourist work.
    void setFrame(const QImage &frame);

private:
    HistogramWidget *m_hist = nullptr;
    WaveformWidget *m_wave = nullptr;
    VectorscopeWidget *m_vector = nullptr;
    QCheckBox *m_vecToggle = nullptr;
    QElapsedTimer m_throttle;
    bool m_showVectorscope = true;
    static constexpr int kThrottleMs = 100;  // ~10 fps refresh
};
