#include "AudioMeterWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QShowEvent>
#include <QtMath>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr float kZeroDb = 0.0f;
constexpr float kYellowThresholdDb = -18.0f;
constexpr float kRedThresholdDb = -6.0f;

QColor zoneColorForDb(float db)
{
    if (db >= kZeroDb) {
        return QColor(255, 191, 71);
    }
    if (db > kRedThresholdDb) {
        return QColor(220, 68, 55);
    }
    if (db > kYellowThresholdDb) {
        return QColor(232, 201, 74);
    }
    return QColor(54, 179, 92);
}

QColor backgroundColor()
{
    return QColor(24, 28, 31);
}

QColor frameColor()
{
    return QColor(54, 59, 64);
}

QColor rmsFillColor()
{
    return QColor(230, 237, 243, 210);
}

QColor holdMarkerColor()
{
    return QColor(250, 250, 250);
}

float normalizedForDb(float db, float minDb, float maxDb)
{
    return std::clamp((db - minDb) / (maxDb - minDb), 0.0f, 1.0f);
}

void fillMeterVertical(QPainter& painter, const QRectF& rect, float startDb, float endDb, float minDb, float maxDb)
{
    const float lo = std::max(minDb, std::min(startDb, endDb));
    const float hi = std::min(maxDb, std::max(startDb, endDb));
    if (hi <= lo) {
        return;
    }

    const std::array<float, 5> boundaries { minDb, kYellowThresholdDb, kRedThresholdDb, kZeroDb, maxDb };
    for (int i = 0; i < static_cast<int>(boundaries.size()) - 1; ++i) {
        const float segmentLo = std::max(lo, boundaries[i]);
        const float segmentHi = std::min(hi, boundaries[i + 1]);
        if (segmentHi <= segmentLo) {
            continue;
        }

        const float topNorm = normalizedForDb(segmentHi, minDb, maxDb);
        const float bottomNorm = normalizedForDb(segmentLo, minDb, maxDb);
        const qreal topY = rect.bottom() - rect.height() * topNorm;
        const qreal bottomY = rect.bottom() - rect.height() * bottomNorm;
        painter.fillRect(QRectF(rect.left(), topY, rect.width(), bottomY - topY),
                         zoneColorForDb((segmentLo + segmentHi) * 0.5f));
    }
}

void fillMeterHorizontal(QPainter& painter, const QRectF& rect, float startDb, float endDb, float minDb, float maxDb)
{
    const float lo = std::max(minDb, std::min(startDb, endDb));
    const float hi = std::min(maxDb, std::max(startDb, endDb));
    if (hi <= lo) {
        return;
    }

    const std::array<float, 5> boundaries { minDb, kYellowThresholdDb, kRedThresholdDb, kZeroDb, maxDb };
    for (int i = 0; i < static_cast<int>(boundaries.size()) - 1; ++i) {
        const float segmentLo = std::max(lo, boundaries[i]);
        const float segmentHi = std::min(hi, boundaries[i + 1]);
        if (segmentHi <= segmentLo) {
            continue;
        }

        const float leftNorm = normalizedForDb(segmentLo, minDb, maxDb);
        const float rightNorm = normalizedForDb(segmentHi, minDb, maxDb);
        const qreal x = rect.left() + rect.width() * leftNorm;
        const qreal w = rect.width() * (rightNorm - leftNorm);
        painter.fillRect(QRectF(x, rect.top(), w, rect.height()), zoneColorForDb((segmentLo + segmentHi) * 0.5f));
    }
}

} // namespace

AudioMeterWidget::AudioMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(8, 40);
    m_decayTimer.setInterval(kTickMs);
    connect(&m_decayTimer, &QTimer::timeout, this, &AudioMeterWidget::onDecayTick);
    m_decayTimer.start();
}

void AudioMeterWidget::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) {
        return;
    }

    m_orientation = orientation;
    updateGeometry();
    update();
}

void AudioMeterWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_decayTimer.start();
}

void AudioMeterWidget::hideEvent(QHideEvent* event)
{
    QWidget::hideEvent(event);
    m_decayTimer.stop();
}

QSize AudioMeterWidget::sizeHint() const
{
    return m_orientation == Qt::Vertical ? QSize(18, 120) : QSize(120, 18);
}

void AudioMeterWidget::setLevels(float pkL, float pkR, float rmsL, float rmsR)
{
    if (!isVisible())
        return;

    updateChannel(m_left, linearToDb(pkL), linearToDb(rmsL));
    updateChannel(m_right, linearToDb(pkR), linearToDb(rmsR));

    if (syncPaintState()) {
        update();
    }
}

void AudioMeterWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), backgroundColor());

    QRectF content = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    painter.setPen(frameColor());
    painter.drawRect(content);

    const auto drawChannel = [&](const QRectF& channelRect, const ChannelState& channel) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(channelRect, 1.5, 1.5);
        painter.save();
        painter.setClipPath(clipPath);
        painter.fillRect(channelRect, QColor(18, 21, 24));

        if (m_orientation == Qt::Vertical) {
            fillMeterVertical(painter, channelRect, m_minDb, channel.peakDb, m_minDb, m_maxDb);

            const float rmsNorm = normalizedForDb(channel.rmsDb, m_minDb, m_maxDb);
            const qreal rmsHeight = channelRect.height() * rmsNorm;
            const QRectF rmsRect(channelRect.left() + channelRect.width() * 0.22,
                                 channelRect.bottom() - rmsHeight,
                                 channelRect.width() * 0.56,
                                 rmsHeight);
            painter.fillRect(rmsRect, rmsFillColor());

            const float holdNorm = normalizedForDb(channel.holdDb, m_minDb, m_maxDb);
            const qreal holdY = channelRect.bottom() - channelRect.height() * holdNorm;
            painter.fillRect(QRectF(channelRect.left(), holdY - 1.0, channelRect.width(), 2.0), holdMarkerColor());
        } else {
            fillMeterHorizontal(painter, channelRect, m_minDb, channel.peakDb, m_minDb, m_maxDb);

            const float rmsNorm = normalizedForDb(channel.rmsDb, m_minDb, m_maxDb);
            const qreal rmsWidth = channelRect.width() * rmsNorm;
            const QRectF rmsRect(channelRect.left(),
                                 channelRect.top() + channelRect.height() * 0.22,
                                 rmsWidth,
                                 channelRect.height() * 0.56);
            painter.fillRect(rmsRect, rmsFillColor());

            const float holdNorm = normalizedForDb(channel.holdDb, m_minDb, m_maxDb);
            const qreal holdX = channelRect.left() + channelRect.width() * holdNorm;
            painter.fillRect(QRectF(holdX - 1.0, channelRect.top(), 2.0, channelRect.height()), holdMarkerColor());
        }

        painter.restore();
        painter.setPen(frameColor());
        painter.drawRoundedRect(channelRect, 1.5, 1.5);
    };

    if (m_orientation == Qt::Vertical) {
        const qreal gap = 2.0;
        const qreal channelWidth = std::max<qreal>(2.0, (content.width() - gap) * 0.5);
        const QRectF leftRect(content.left(), content.top(), channelWidth, content.height());
        const QRectF rightRect(leftRect.right() + gap, content.top(), channelWidth, content.height());
        drawChannel(leftRect, m_left);
        drawChannel(rightRect, m_right);
    } else {
        const qreal gap = 2.0;
        const qreal channelHeight = std::max<qreal>(2.0, (content.height() - gap) * 0.5);
        const QRectF topRect(content.left(), content.top(), content.width(), channelHeight);
        const QRectF bottomRect(content.left(), topRect.bottom() + gap, content.width(), channelHeight);
        drawChannel(topRect, m_left);
        drawChannel(bottomRect, m_right);
    }
}

void AudioMeterWidget::onDecayTick()
{
    decayChannel(m_left);
    decayChannel(m_right);

    if (syncPaintState()) {
        update();
    }
}

float AudioMeterWidget::clampDb(float db)
{
    return std::clamp(db, kMinDb, kMaxDb);
}

float AudioMeterWidget::linearToDb(float linear)
{
    if (!(linear > 0.0f)) {
        return kMinDb;
    }

    return clampDb(20.0f * std::log10(linear));
}

bool AudioMeterWidget::differsForPaint(float a, float b)
{
    return std::fabs(a - b) >= kRepaintThresholdDb;
}

void AudioMeterWidget::updateChannel(ChannelState& channel, float peakDb, float rmsDb)
{
    channel.peakDb = clampDb(peakDb);
    channel.rmsDb = clampDb(std::min(rmsDb, channel.peakDb));

    if (channel.peakDb >= channel.holdDb) {
        channel.holdDb = channel.peakDb;
        channel.holdElapsedMs = 0;
    }
}

void AudioMeterWidget::decayChannel(ChannelState& channel)
{
    channel.peakDb = clampDb(channel.peakDb - kDecayDbPerTick);

    channel.holdElapsedMs += kTickMs;
    if (channel.holdElapsedMs > peakHoldMs()) {
        channel.holdDb = clampDb(channel.holdDb - kDecayDbPerTick);
    }

    if (channel.holdDb < channel.peakDb) {
        channel.holdDb = channel.peakDb;
    }

    if (channel.rmsDb > channel.peakDb) {
        channel.rmsDb = channel.peakDb;
    }
}

bool AudioMeterWidget::syncPaintState()
{
    const auto updateLastPainted = [](ChannelState& channel) {
        bool changed = false;
        if (differsForPaint(channel.lastPaintedPeakDb, channel.peakDb)) {
            channel.lastPaintedPeakDb = channel.peakDb;
            changed = true;
        }
        if (differsForPaint(channel.lastPaintedRmsDb, channel.rmsDb)) {
            channel.lastPaintedRmsDb = channel.rmsDb;
            changed = true;
        }
        if (differsForPaint(channel.lastPaintedHoldDb, channel.holdDb)) {
            channel.lastPaintedHoldDb = channel.holdDb;
            changed = true;
        }
        return changed;
    };

    const bool leftChanged = updateLastPainted(m_left);
    const bool rightChanged = updateLastPainted(m_right);
    return leftChanged || rightChanged;
}

double AudioMeterWidget::currentPeakHoldDb() const
{
    return std::max(m_left.holdDb, m_right.holdDb);
}

void AudioMeterWidget::resetPeakHold()
{
    m_left.holdDb = -120.0f;
    m_right.holdDb = -120.0f;
    m_left.holdElapsedMs = 0;
    m_right.holdElapsedMs = 0;
    update();
}

void AudioMeterWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);

    if (m_trackIndex >= 0) {
        auto* eqAction = menu.addAction(QStringLiteral("EQ \u30D7\u30EA\u30BB\u30C3\u30C8"));
        connect(eqAction, &QAction::triggered, this, [this, event]() {
            emit requestEqPresetMenu(m_trackIndex, event->globalPos());
        });

        auto* compAction = menu.addAction(QStringLiteral("\u30B3\u30F3\u30D7\u30EC\u30C3\u30B5\u30FC\u8A2D\u5B9A..."));
        connect(compAction, &QAction::triggered, this, [this]() {
            emit requestCompressorDialog();
        });

        auto* duckAction = menu.addAction(QStringLiteral("\u30AA\u30FC\u30C8\u30C0\u30C3\u30AF\u8A2D\u5B9A..."));
        connect(duckAction, &QAction::triggered, this, [this]() {
            emit requestAutoDuckDialog();
        });

        auto* normAction = menu.addAction(QStringLiteral("\u30CE\u30FC\u30DE\u30E9\u30A4\u30BA"));
        connect(normAction, &QAction::triggered, this, [this]() {
            const double peak = currentPeakHoldDb();
            if (peak <= kMinDb) {
                emit requestNormalize(m_trackIndex, 0.0);
                return;
            }
            const double gainDb = std::clamp(-1.0 - peak, -24.0, 12.0);
            emit requestNormalize(m_trackIndex, gainDb);
        });

        menu.addSeparator();

        auto* resetAction = menu.addAction(QStringLiteral("\u30E1\u30FC\u30BF\u30FC\u3092\u30EA\u30BB\u30C3\u30C8"));
        connect(resetAction, &QAction::triggered, this, &AudioMeterWidget::resetPeakHold);
    } else {
        auto* compAction = menu.addAction(QStringLiteral("\u30DE\u30B9\u30BF\u30FC\u30B3\u30F3\u30D7\u30EC\u30C3\u30B5\u30FC\u8A2D\u5B9A..."));
        connect(compAction, &QAction::triggered, this, [this]() {
            emit requestCompressorDialog();
        });

        auto* normAllAction = menu.addAction(QStringLiteral("\u30CE\u30FC\u30DE\u30E9\u30A4\u30BA (\u5168\u30C8\u30E9\u30C3\u30AF)"));
        connect(normAllAction, &QAction::triggered, this, [this]() {
            emit requestNormalizeAll();
        });

        menu.addSeparator();

        auto* resetAction = menu.addAction(QStringLiteral("\u30E1\u30FC\u30BF\u30FC\u3092\u30EA\u30BB\u30C3\u30C8"));
        connect(resetAction, &QAction::triggered, this, [this]() {
            emit requestResetAllMeters();
        });
    }

    menu.exec(event->globalPos());
}
