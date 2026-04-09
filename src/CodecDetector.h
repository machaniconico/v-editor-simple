#pragma once

#include <QString>
#include <QVector>
#include <QPair>

extern "C" {
#include <libavcodec/avcodec.h>
}

struct CodecOption {
    QString name;       // Display name
    QString ffmpegName; // FFmpeg encoder name
    bool available;
    int quality;        // 1-5 stars
};

class CodecDetector
{
public:
    static bool isEncoderAvailable(const QString &name);

    static QVector<CodecOption> availableVideoEncoders();
    static QVector<CodecOption> availableAudioEncoders();

    static QString bestAACEncoder();
    static QString bestVideoEncoder(const QString &codecFamily);

    static QVector<CodecOption> hwAccelVideoEncoders();
};
