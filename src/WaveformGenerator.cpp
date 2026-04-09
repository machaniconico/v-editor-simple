#include "WaveformGenerator.h"
#include <QThread>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

WaveformGenerator::WaveformGenerator(QObject *parent)
    : QObject(parent) {}

void WaveformGenerator::generateAsync(const QString &filePath, int peaksPerSecond)
{
    auto *thread = QThread::create([this, filePath, peaksPerSecond]() {
        WaveformData data = generate(filePath, peaksPerSecond);
        emit waveformReady(filePath, data);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

WaveformData WaveformGenerator::generate(const QString &filePath, int peaksPerSecond)
{
    QVector<float> samples;
    int sampleRate = 0;

    if (!decodeAudio(filePath, samples, sampleRate))
        return {};

    double duration = samples.isEmpty() ? 0.0 : static_cast<double>(samples.size()) / sampleRate;
    return buildPeaks(samples, sampleRate, duration, peaksPerSecond);
}

bool WaveformGenerator::decodeAudio(const QString &filePath, QVector<float> &samples, int &sampleRate)
{
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) < 0)
        return false;

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Find audio stream
    int audioIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioIdx = static_cast<int>(i);
            break;
        }
    }
    if (audioIdx < 0) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    auto *codecpar = fmtCtx->streams[audioIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecContext *decCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(decCtx, codecpar);
    if (avcodec_open2(decCtx, codec, nullptr) < 0) {
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Setup resampler to mono float
    SwrContext *swrCtx = nullptr;
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_MONO;
    swr_alloc_set_opts2(&swrCtx,
        &outLayout, AV_SAMPLE_FMT_FLT, decCtx->sample_rate,
        &decCtx->ch_layout, decCtx->sample_fmt, decCtx->sample_rate,
        0, nullptr);

    if (!swrCtx || swr_init(swrCtx) < 0) {
        if (swrCtx) swr_free(&swrCtx);
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    sampleRate = decCtx->sample_rate;

    // Decode all audio
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    samples.reserve(sampleRate * 300); // pre-alloc for up to 5 min

    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != audioIdx) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(decCtx, packet) == 0) {
            while (avcodec_receive_frame(decCtx, frame) == 0) {
                // Resample to mono float
                int outSamples = swr_get_out_samples(swrCtx, frame->nb_samples);
                QVector<float> buf(outSamples);
                uint8_t *outBuf = reinterpret_cast<uint8_t*>(buf.data());
                int converted = swr_convert(swrCtx, &outBuf, outSamples,
                    const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
                if (converted > 0) {
                    buf.resize(converted);
                    samples.append(buf);
                }
            }
        }
        av_packet_unref(packet);
    }

    // Flush
    avcodec_send_packet(decCtx, nullptr);
    while (avcodec_receive_frame(decCtx, frame) == 0) {
        int outSamples = swr_get_out_samples(swrCtx, frame->nb_samples);
        QVector<float> buf(outSamples);
        uint8_t *outBuf = reinterpret_cast<uint8_t*>(buf.data());
        int converted = swr_convert(swrCtx, &outBuf, outSamples,
            const_cast<const uint8_t**>(frame->extended_data), frame->nb_samples);
        if (converted > 0) {
            buf.resize(converted);
            samples.append(buf);
        }
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swrCtx);
    avcodec_free_context(&decCtx);
    avformat_close_input(&fmtCtx);

    return !samples.isEmpty();
}

WaveformData WaveformGenerator::buildPeaks(const QVector<float> &samples, int sampleRate,
                                            double duration, int peaksPerSecond)
{
    WaveformData data;
    data.sampleRate = sampleRate;
    data.duration = duration;
    data.peaksPerSecond = peaksPerSecond;

    int totalPeaks = static_cast<int>(duration * peaksPerSecond);
    if (totalPeaks <= 0 || samples.isEmpty()) return data;

    int samplesPerPeak = samples.size() / totalPeaks;
    if (samplesPerPeak < 1) samplesPerPeak = 1;

    data.peaks.resize(totalPeaks);

    for (int i = 0; i < totalPeaks; ++i) {
        int start = i * samplesPerPeak;
        int end = qMin(start + samplesPerPeak, samples.size());
        float maxAmp = 0.0f;
        for (int j = start; j < end; ++j)
            maxAmp = qMax(maxAmp, std::abs(samples[j]));
        data.peaks[i] = qMin(maxAmp, 1.0f);
    }

    return data;
}
