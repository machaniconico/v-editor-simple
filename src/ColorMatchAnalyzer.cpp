#include "ColorMatchAnalyzer.h"
#include <QtDebug>
#include <cmath>

namespace colormatch::analyze {

// ---------------------------------------------------------------------------
// analyzeImage
// ---------------------------------------------------------------------------
ColorStats analyzeImage(const QImage &img)
{
    if (img.isNull())
        return ColorStats{};

    // Convert to ARGB32 for uniform pixel access
    const QImage src = img.format() == QImage::Format_ARGB32
                       ? img
                       : img.convertToFormat(QImage::Format_ARGB32);

    ColorStats stats;
    const int w = src.width();
    const int h = src.height();

    double rSum = 0.0, gSum = 0.0, bSum = 0.0;
    double rSumSq = 0.0, gSumSq = 0.0, bSumSq = 0.0;
    qint64 count = 0;

    for (int y = 0; y < h; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int r = qRed(px);
            const int g = qGreen(px);
            const int b = qBlue(px);

            stats.rHist[r]++;
            stats.gHist[g]++;
            stats.bHist[b]++;

            rSum   += r;
            gSum   += g;
            bSum   += b;
            rSumSq += static_cast<double>(r) * r;
            gSumSq += static_cast<double>(g) * g;
            bSumSq += static_cast<double>(b) * b;
            ++count;
        }
    }

    if (count == 0)
        return stats;

    stats.sampleCount = count;
    const double n = static_cast<double>(count);

    stats.rMean = rSum / n;
    stats.gMean = gSum / n;
    stats.bMean = bSum / n;

    // std = sqrt(E[X^2] - (E[X])^2)
    stats.rStd = std::sqrt(std::max(0.0, rSumSq / n - stats.rMean * stats.rMean));
    stats.gStd = std::sqrt(std::max(0.0, gSumSq / n - stats.gMean * stats.gMean));
    stats.bStd = std::sqrt(std::max(0.0, bSumSq / n - stats.bMean * stats.bMean));

    // BT.709 luminance
    stats.luminance = 0.2126 * stats.rMean + 0.7152 * stats.gMean + 0.0722 * stats.bMean;

    return stats;
}

// ---------------------------------------------------------------------------
// analyzeFrameRange
// ---------------------------------------------------------------------------
ColorStats analyzeFrameRange(const QString &videoPath, int startFrameNo, int endFrameNo)
{
    // Empty range
    if (endFrameNo <= startFrameNo)
        return ColorStats{};

    // Open input
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, videoPath.toUtf8().constData(), nullptr, nullptr) < 0) {
        qWarning("ColorMatchAnalyzer: cannot open video '%s'", videoPath.toUtf8().constData());
        return ColorStats{};
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        qWarning("ColorMatchAnalyzer: cannot find stream info for '%s'", videoPath.toUtf8().constData());
        avformat_close_input(&fmtCtx);
        return ColorStats{};
    }

    // Find best video stream
    int videoStreamIdx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIdx = static_cast<int>(i);
            break;
        }
    }
    if (videoStreamIdx < 0) {
        qWarning("ColorMatchAnalyzer: no video stream in '%s'", videoPath.toUtf8().constData());
        avformat_close_input(&fmtCtx);
        return ColorStats{};
    }

    AVCodecParameters *codecpar = fmtCtx->streams[videoStreamIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        qWarning("ColorMatchAnalyzer: no decoder for codec id %d", codecpar->codec_id);
        avformat_close_input(&fmtCtx);
        return ColorStats{};
    }

    AVCodecContext *decCtx = avcodec_alloc_context3(codec);
    if (!decCtx) {
        avformat_close_input(&fmtCtx);
        return ColorStats{};
    }
    avcodec_parameters_to_context(decCtx, codecpar);
    if (avcodec_open2(decCtx, codec, nullptr) < 0) {
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmtCtx);
        return ColorStats{};
    }

    // Allocate packet and frames
    AVPacket *packet  = av_packet_alloc();
    AVFrame  *frame   = av_frame_alloc();
    AVFrame  *rgbFrame = av_frame_alloc();

    // Accumulators for weighted stats
    // We accumulate per-pixel sums across all frames.
    QVector<int> rHist(256, 0), gHist(256, 0), bHist(256, 0);
    double rSum = 0.0, gSum = 0.0, bSum = 0.0;
    double rSumSq = 0.0, gSumSq = 0.0, bSumSq = 0.0;
    qint64 totalPixels = 0;
    int frameDecoded = 0;

    SwsContext *swsCtx = nullptr;
    int swsW = -1, swsH = -1;

    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != videoStreamIdx) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(decCtx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (avcodec_receive_frame(decCtx, frame) == 0) {
            const int fn = frameDecoded;
            ++frameDecoded;

            if (fn < startFrameNo || fn >= endFrameNo) {
                av_frame_unref(frame);
                continue;
            }

            // Build / rebuild sws context if dimensions changed
            if (swsW != frame->width || swsH != frame->height) {
                if (swsCtx) sws_freeContext(swsCtx);
                swsW = frame->width;
                swsH = frame->height;
                swsCtx = sws_getContext(
                    swsW, swsH, static_cast<AVPixelFormat>(frame->format),
                    swsW, swsH, AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);

                // Allocate rgbFrame buffer
                av_frame_unref(rgbFrame);
                rgbFrame->format = AV_PIX_FMT_RGB24;
                rgbFrame->width  = swsW;
                rgbFrame->height = swsH;
                av_frame_get_buffer(rgbFrame, 0);
            }

            if (!swsCtx) {
                av_frame_unref(frame);
                continue;
            }

            sws_scale(swsCtx,
                      frame->data, frame->linesize, 0, frame->height,
                      rgbFrame->data, rgbFrame->linesize);

            // Wrap as QImage (Format_RGB888 = packed RGB24, no copy)
            QImage qimg(rgbFrame->data[0],
                        rgbFrame->width, rgbFrame->height,
                        rgbFrame->linesize[0],
                        QImage::Format_RGB888);

            // analyzeImage on this frame's QImage
            ColorStats fs = analyzeImage(qimg);

            // Accumulate pixel-count-weighted stats
            if (fs.sampleCount > 0) {
                for (int i = 0; i < 256; ++i) {
                    rHist[i] += fs.rHist[i];
                    gHist[i] += fs.gHist[i];
                    bHist[i] += fs.bHist[i];
                }
                const double n = static_cast<double>(fs.sampleCount);
                rSum   += fs.rMean * n;
                gSum   += fs.gMean * n;
                bSum   += fs.bMean * n;
                // E[X^2] = Var(X) + (E[X])^2
                rSumSq += (fs.rStd * fs.rStd + fs.rMean * fs.rMean) * n;
                gSumSq += (fs.gStd * fs.gStd + fs.gMean * fs.gMean) * n;
                bSumSq += (fs.bStd * fs.bStd + fs.bMean * fs.bMean) * n;
                totalPixels += fs.sampleCount;
            }

            av_frame_unref(frame);

            // Early exit once we have passed the requested range
            if (fn >= endFrameNo - 1 && frameDecoded >= endFrameNo)
                break;
        }

        if (frameDecoded >= endFrameNo)
            break;
    }

    // Clean up
    if (swsCtx) sws_freeContext(swsCtx);
    av_frame_free(&rgbFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decCtx);
    avformat_close_input(&fmtCtx);

    if (totalPixels == 0)
        return ColorStats{};

    ColorStats result;
    result.rHist = rHist;
    result.gHist = gHist;
    result.bHist = bHist;
    result.sampleCount = totalPixels;

    const double N = static_cast<double>(totalPixels);
    result.rMean = rSum / N;
    result.gMean = gSum / N;
    result.bMean = bSum / N;
    result.rStd  = std::sqrt(std::max(0.0, rSumSq / N - result.rMean * result.rMean));
    result.gStd  = std::sqrt(std::max(0.0, gSumSq / N - result.gMean * result.gMean));
    result.bStd  = std::sqrt(std::max(0.0, bSumSq / N - result.bMean * result.bMean));
    result.luminance = 0.2126 * result.rMean + 0.7152 * result.gMean + 0.0722 * result.bMean;

    return result;
}

} // namespace colormatch::analyze
