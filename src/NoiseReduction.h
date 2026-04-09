#pragma once

#include <QObject>
#include <QThread>
#include <QString>

// --- Noise Reduction Types ---

enum class NoiseReductionType {
    AudioDenoise,
    VideoDenoise,
    AudioVideoDenoise
};

// --- Audio Denoise Configuration ---

struct AudioDenoiseConfig {
    double noiseFloor = -40.0;      // -80 to 0 dB
    double reductionAmount = 0.5;   // 0.0 to 1.0
    bool adaptiveMode = true;       // enable adaptive noise profiling

    bool isDefault() const {
        return noiseFloor == -40.0 && reductionAmount == 0.5 && adaptiveMode == true;
    }
};

// --- Video Denoise Configuration ---

enum class VideoDenoiseMethod {
    HQDN3D,
    NLMeans
};

struct VideoDenoiseConfig {
    double spatialStrength = 4.0;   // 0 to 30
    double temporalStrength = 6.0;  // 0 to 30
    VideoDenoiseMethod method = VideoDenoiseMethod::HQDN3D;

    bool isDefault() const {
        return spatialStrength == 4.0 && temporalStrength == 6.0
            && method == VideoDenoiseMethod::HQDN3D;
    }
};

// --- Audio Denoise Preview Result ---

struct AudioDenoisePreview {
    double originalNoiseLevel = 0.0;    // dB
    double denoisedNoiseLevel = 0.0;    // dB
    double reductionDb = 0.0;           // dB reduced
    bool success = false;
};

// --- Noise Reduction Processor ---

class NoiseReduction : public QObject
{
    Q_OBJECT

public:
    explicit NoiseReduction(QObject *parent = nullptr);

    // Apply audio noise reduction using FFmpeg afftdn filter
    void denoiseAudio(const QString &inputPath, const QString &outputPath,
                      const AudioDenoiseConfig &config = {});

    // Apply video noise reduction using FFmpeg hqdn3d or nlmeans filter
    void denoiseVideo(const QString &inputPath, const QString &outputPath,
                      const VideoDenoiseConfig &config = {});

    // Apply both audio and video noise reduction
    void denoiseAll(const QString &inputPath, const QString &outputPath,
                    const AudioDenoiseConfig &audioConfig = {},
                    const VideoDenoiseConfig &videoConfig = {});

    // Analyze a short segment and return before/after noise levels
    AudioDenoisePreview previewAudioDenoise(const QString &inputPath,
                                            const AudioDenoiseConfig &config = {});

    void cancel();

signals:
    void progressChanged(int percent);
    void denoiseComplete(bool success, const QString &message);

private:
    // Build FFmpeg filter description strings
    static QString buildAudioFilterDesc(const AudioDenoiseConfig &config);
    static QString buildVideoFilterDesc(const VideoDenoiseConfig &config);

    // Core processing: read packets -> filter graph -> write output
    bool processWithFilter(const QString &inputPath, const QString &outputPath,
                           const QString &audioFilter, const QString &videoFilter);

    // Find stream index by media type
    static int findStreamIndex(struct AVFormatContext *fmtCtx, int mediaType);

    // Measure RMS noise level of audio (in dB) for a short segment
    static double measureNoiseLevel(const QString &filePath, double startSec, double durationSec);

    bool m_cancelled = false;
    QThread *m_thread = nullptr;
};
