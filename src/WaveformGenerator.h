#pragma once

#include <QString>
#include <QVector>
#include <QObject>

struct WaveformData {
    QVector<float> peaks;    // normalized 0.0-1.0 (absolute amplitude)
    int sampleRate = 0;
    double duration = 0.0;
    int peaksPerSecond = 0;  // resolution

    bool isEmpty() const { return peaks.isEmpty(); }
};

class WaveformGenerator : public QObject
{
    Q_OBJECT

public:
    explicit WaveformGenerator(QObject *parent = nullptr);

    // Generate waveform synchronously (for background thread)
    static WaveformData generate(const QString &filePath, int peaksPerSecond = 50);

    // Generate async
    void generateAsync(const QString &filePath, int peaksPerSecond = 50);

signals:
    void waveformReady(const QString &filePath, const WaveformData &data);

private:
    static bool decodeAudio(const QString &filePath, QVector<float> &samples, int &sampleRate);
    static WaveformData buildPeaks(const QVector<float> &samples, int sampleRate,
                                    double duration, int peaksPerSecond);
};
