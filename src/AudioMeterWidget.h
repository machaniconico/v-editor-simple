#pragma once

#include <QTimer>
#include <QWidget>

class AudioMeterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AudioMeterWidget(QWidget* parent = nullptr);

    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }
    int peakHoldMs() const { return 1500; }
    QSize sizeHint() const override;

public slots:
    void setLevels(float pkL, float pkR, float rmsL, float rmsR);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onDecayTick();

private:
    static constexpr int kTickMs = 50;
    static constexpr float kDecayDbPerTick = 1.7f;
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 3.0f;
    static constexpr float kRepaintThresholdDb = 0.5f;

    struct ChannelState {
        float peakDb = kMinDb;
        float rmsDb = kMinDb;
        float holdDb = kMinDb;
        float lastPaintedPeakDb = kMinDb;
        float lastPaintedRmsDb = kMinDb;
        float lastPaintedHoldDb = kMinDb;
        qint64 holdElapsedMs = 0;
    };

    static float clampDb(float db);
    static float linearToDb(float linear);
    static bool differsForPaint(float a, float b);

    void updateChannel(ChannelState& channel, float peakDb, float rmsDb);
    void decayChannel(ChannelState& channel);
    bool syncPaintState();

    QTimer m_decayTimer;
    Qt::Orientation m_orientation = Qt::Vertical;
    ChannelState m_left;
    ChannelState m_right;
    float m_minDb = kMinDb;
    float m_maxDb = kMaxDb;
};
