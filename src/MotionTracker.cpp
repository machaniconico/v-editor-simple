#include "MotionTracker.h"
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

// ---------------------------------------------------------------------------
// TrackingResult
// ---------------------------------------------------------------------------

QRect TrackingResult::positionAtTime(double timeSec) const
{
    if (regions.isEmpty() || fps <= 0.0)
        return {};

    double frameIdx = timeSec * fps - startFrame;
    if (frameIdx <= 0.0)
        return regions.first().rect;
    if (frameIdx >= regions.size() - 1)
        return regions.last().rect;

    int lo = static_cast<int>(std::floor(frameIdx));
    int hi = lo + 1;
    double t = frameIdx - lo;

    const QRect &a = regions[lo].rect;
    const QRect &b = regions[hi].rect;

    // Linear interpolation
    int x = static_cast<int>(a.x() + (b.x() - a.x()) * t);
    int y = static_cast<int>(a.y() + (b.y() - a.y()) * t);
    int w = static_cast<int>(a.width() + (b.width() - a.width()) * t);
    int h = static_cast<int>(a.height() + (b.height() - a.height()) * t);

    return QRect(x, y, w, h);
}

// ---------------------------------------------------------------------------
// MotionTracker
// ---------------------------------------------------------------------------

MotionTracker::MotionTracker(QObject *parent)
    : QObject(parent) {}

void MotionTracker::setSearchMargin(int margin)
{
    m_searchMargin = qMax(10, margin);
}

void MotionTracker::setMinConfidence(double conf)
{
    m_minConfidence = qBound(0.0, conf, 1.0);
}

// ---------------------------------------------------------------------------
// Public: startTracking
// ---------------------------------------------------------------------------

void MotionTracker::startTracking(const QString &filePath, const QRect &initialRect)
{
    m_result = TrackingResult{};

    auto *thread = QThread::create([this, filePath, initialRect]() {
        decodeAndTrack(filePath, initialRect);
        emit trackingComplete(m_result);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

// ---------------------------------------------------------------------------
// Public: trackFrame — single-frame template matching
// ---------------------------------------------------------------------------

TrackingRegion MotionTracker::trackFrame(const QImage &currentFrame,
                                         const QImage &templateImage,
                                         const QRect &searchArea)
{
    TrackingRegion best;
    best.confidence = -1.0;

    QImage grayFrame = toGrayscale(currentFrame);
    QImage grayTempl = toGrayscale(templateImage);

    int tw = grayTempl.width();
    int th = grayTempl.height();

    // Clamp search area to frame bounds
    int sx = qMax(0, searchArea.x());
    int sy = qMax(0, searchArea.y());
    int ex = qMin(grayFrame.width() - tw, searchArea.x() + searchArea.width() - tw);
    int ey = qMin(grayFrame.height() - th, searchArea.y() + searchArea.height() - th);

    if (ex < sx || ey < sy)
        return best;

    // Slide template over search area, compute NCC at each position
    for (int y = sy; y <= ey; ++y) {
        for (int x = sx; x <= ex; ++x) {
            double score = computeNCC(grayFrame, grayTempl, x, y);
            if (score > best.confidence) {
                best.confidence = score;
                best.rect = QRect(x, y, tw, th);
            }
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// Public: applyToOverlay
// ---------------------------------------------------------------------------

QRectF MotionTracker::applyToOverlay(const TrackingResult &trackingData,
                                     const QRectF &overlayRect,
                                     double currentTime,
                                     int videoWidth, int videoHeight)
{
    if (trackingData.isEmpty() || videoWidth <= 0 || videoHeight <= 0)
        return overlayRect;

    QRect tracked = trackingData.positionAtTime(currentTime);
    if (tracked.isNull())
        return overlayRect;

    // Convert tracked pixel position to normalized 0.0-1.0 coordinates
    double nx = static_cast<double>(tracked.x()) / videoWidth;
    double ny = static_cast<double>(tracked.y()) / videoHeight;

    // Offset overlay so its center follows the tracked center
    double cx = nx + static_cast<double>(tracked.width()) / (2.0 * videoWidth);
    double cy = ny + static_cast<double>(tracked.height()) / (2.0 * videoHeight);

    return QRectF(cx - overlayRect.width() / 2.0,
                  cy - overlayRect.height() / 2.0,
                  overlayRect.width(),
                  overlayRect.height());
}

// ---------------------------------------------------------------------------
// JSON export / import
// ---------------------------------------------------------------------------

bool MotionTracker::exportTrackingData(const TrackingResult &data, const QString &filePath)
{
    QJsonObject root;
    root["startFrame"] = data.startFrame;
    root["endFrame"] = data.endFrame;
    root["fps"] = data.fps;

    QJsonArray arr;
    for (const auto &r : data.regions) {
        QJsonObject obj;
        obj["x"] = r.rect.x();
        obj["y"] = r.rect.y();
        obj["w"] = r.rect.width();
        obj["h"] = r.rect.height();
        obj["confidence"] = r.confidence;
        obj["frame"] = r.frameNumber;
        arr.append(obj);
    }
    root["regions"] = arr;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

TrackingResult MotionTracker::importTrackingData(const QString &filePath)
{
    TrackingResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return result;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return result;

    QJsonObject root = doc.object();
    result.startFrame = root["startFrame"].toInt();
    result.endFrame = root["endFrame"].toInt();
    result.fps = root["fps"].toDouble();

    QJsonArray arr = root["regions"].toArray();
    for (const auto &v : arr) {
        QJsonObject obj = v.toObject();
        TrackingRegion r;
        r.rect = QRect(obj["x"].toInt(), obj["y"].toInt(),
                        obj["w"].toInt(), obj["h"].toInt());
        r.confidence = obj["confidence"].toDouble();
        r.frameNumber = obj["frame"].toInt();
        result.regions.append(r);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Private: decodeAndTrack — FFmpeg frame extraction + tracking loop
// ---------------------------------------------------------------------------

bool MotionTracker::decodeAndTrack(const QString &filePath, const QRect &initialRect)
{
    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, filePath.toUtf8().constData(), nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    // Find video stream
    int videoIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoIdx = static_cast<int>(i);
            break;
        }
    }
    if (videoIdx < 0) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    auto *codecpar = fmtCtx->streams[videoIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) { avformat_close_input(&fmtCtx); return false; }

    AVCodecContext *decCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(decCtx, codecpar);
    if (avcodec_open2(decCtx, codec, nullptr) < 0) {
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    int frameW = decCtx->width;
    int frameH = decCtx->height;

    // SwsContext to convert decoded frames to RGB32 QImage
    SwsContext *swsCtx = sws_getContext(
        frameW, frameH, decCtx->pix_fmt,
        frameW, frameH, AV_PIX_FMT_RGB32,
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx) {
        avcodec_free_context(&decCtx);
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVStream *stream = fmtCtx->streams[videoIdx];
    double fps = av_q2d(stream->avg_frame_rate);
    if (fps <= 0.0) fps = 25.0;

    // Estimate total frames for progress
    int64_t totalFrames = stream->nb_frames;
    if (totalFrames <= 0 && stream->duration > 0)
        totalFrames = static_cast<int64_t>(stream->duration * av_q2d(stream->time_base) * fps);
    if (totalFrames <= 0) totalFrames = 1;

    m_result.fps = fps;
    m_result.startFrame = 0;

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    QImage templateImage;   // extracted from first frame
    QRect lastRect = initialRect;
    int frameCount = 0;
    bool firstFrame = true;

    while (av_read_frame(fmtCtx, packet) >= 0) {
        if (packet->stream_index != videoIdx) {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(decCtx, packet) == 0) {
            while (avcodec_receive_frame(decCtx, frame) == 0) {
                // Convert to QImage
                QImage qimg(frameW, frameH, QImage::Format_RGB32);
                uint8_t *dest[1] = { qimg.bits() };
                int destLinesize[1] = { static_cast<int>(qimg.bytesPerLine()) };
                sws_scale(swsCtx, frame->data, frame->linesize, 0,
                          frameH, dest, destLinesize);

                if (firstFrame) {
                    // Extract template from initial rectangle
                    QRect clamped = initialRect.intersected(qimg.rect());
                    if (clamped.isEmpty()) {
                        // Bad initial rect — abort
                        av_packet_unref(packet);
                        goto cleanup;
                    }
                    templateImage = qimg.copy(clamped);
                    lastRect = clamped;

                    TrackingRegion region;
                    region.rect = clamped;
                    region.confidence = 1.0;
                    region.frameNumber = 0;
                    m_result.regions.append(region);

                    firstFrame = false;
                } else {
                    // Build search area around last known position
                    QRect search(
                        lastRect.x() - m_searchMargin,
                        lastRect.y() - m_searchMargin,
                        lastRect.width() + 2 * m_searchMargin,
                        lastRect.height() + 2 * m_searchMargin);

                    TrackingRegion region = trackFrame(qimg, templateImage, search);
                    region.frameNumber = frameCount;

                    if (region.confidence >= m_minConfidence) {
                        lastRect = region.rect;
                    } else {
                        // Low confidence — keep last known position
                        region.rect = lastRect;
                    }

                    m_result.regions.append(region);
                }

                frameCount++;

                // Report progress
                int pct = static_cast<int>(100.0 * frameCount / totalFrames);
                emit progressChanged(qMin(pct, 100));
            }
        }
        av_packet_unref(packet);
    }

cleanup:
    m_result.endFrame = frameCount > 0 ? frameCount - 1 : 0;

    av_frame_free(&frame);
    av_packet_free(&packet);
    if (swsCtx) sws_freeContext(swsCtx);
    avcodec_free_context(&decCtx);
    avformat_close_input(&fmtCtx);

    return !m_result.isEmpty();
}

// ---------------------------------------------------------------------------
// Private: computeNCC — normalized cross-correlation
// ---------------------------------------------------------------------------

double MotionTracker::computeNCC(const QImage &frame, const QImage &templ,
                                 int offsetX, int offsetY)
{
    int tw = templ.width();
    int th = templ.height();

    // Verify bounds
    if (offsetX < 0 || offsetY < 0 ||
        offsetX + tw > frame.width() || offsetY + th > frame.height())
        return -1.0;

    // Compute means
    double sumF = 0.0, sumT = 0.0;
    int count = tw * th;

    for (int y = 0; y < th; ++y) {
        const uchar *fRow = frame.constScanLine(offsetY + y);
        const uchar *tRow = templ.constScanLine(y);
        for (int x = 0; x < tw; ++x) {
            sumF += fRow[offsetX + x];
            sumT += tRow[x];
        }
    }

    double meanF = sumF / count;
    double meanT = sumT / count;

    // Compute NCC
    double num = 0.0, denF = 0.0, denT = 0.0;

    for (int y = 0; y < th; ++y) {
        const uchar *fRow = frame.constScanLine(offsetY + y);
        const uchar *tRow = templ.constScanLine(y);
        for (int x = 0; x < tw; ++x) {
            double f = fRow[offsetX + x] - meanF;
            double t = tRow[x] - meanT;
            num += f * t;
            denF += f * f;
            denT += t * t;
        }
    }

    double den = std::sqrt(denF * denT);
    if (den < 1e-10) return 0.0;

    return num / den;
}

// ---------------------------------------------------------------------------
// Private: toGrayscale
// ---------------------------------------------------------------------------

QImage MotionTracker::toGrayscale(const QImage &image)
{
    if (image.format() == QImage::Format_Grayscale8)
        return image;

    return image.convertToFormat(QImage::Format_Grayscale8);
}
