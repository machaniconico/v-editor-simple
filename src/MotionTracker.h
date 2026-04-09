#pragma once

#include <QObject>
#include <QRect>
#include <QRectF>
#include <QImage>
#include <QVector>
#include <QString>

// --- Tracking region at a single frame ---

struct TrackingRegion {
    QRect rect;             // bounding box in pixel coordinates
    double confidence = 0.0; // NCC score 0.0-1.0
    int frameNumber = 0;
};

// --- Full tracking result over time ---

struct TrackingResult {
    QVector<TrackingRegion> regions;
    int startFrame = 0;
    int endFrame = 0;
    double fps = 0.0;

    bool isEmpty() const { return regions.isEmpty(); }

    // Get interpolated position at a given time (seconds)
    QRect positionAtTime(double timeSec) const;
};

// --- Motion tracker using template matching (NCC on QImage) ---

class MotionTracker : public QObject
{
    Q_OBJECT

public:
    explicit MotionTracker(QObject *parent = nullptr);

    // Configuration
    void setSearchMargin(int margin);   // pixels to expand search from last position
    int searchMargin() const { return m_searchMargin; }

    void setMinConfidence(double conf); // minimum NCC to accept a match
    double minConfidence() const { return m_minConfidence; }

    // Start tracking: extract frames via FFmpeg and track the object in initialRect
    void startTracking(const QString &filePath, const QRect &initialRect);

    // Track object in a single frame given a template image
    TrackingRegion trackFrame(const QImage &currentFrame, const QImage &templateImage,
                              const QRect &searchArea);

    // Retrieve all tracked positions
    TrackingResult getTrackingData() const { return m_result; }

    // Calculate overlay position based on tracking data at a given time
    static QRectF applyToOverlay(const TrackingResult &trackingData,
                                 const QRectF &overlayRect,
                                 double currentTime,
                                 int videoWidth, int videoHeight);

    // JSON export / import
    static bool exportTrackingData(const TrackingResult &data, const QString &filePath);
    static TrackingResult importTrackingData(const QString &filePath);

signals:
    void progressChanged(int percent);
    void trackingComplete(const TrackingResult &result);

private:
    // Decode video frames via FFmpeg and run tracking loop
    bool decodeAndTrack(const QString &filePath, const QRect &initialRect);

    // Normalized cross-correlation on grayscale data
    static double computeNCC(const QImage &frame, const QImage &templ,
                             int offsetX, int offsetY);

    // Convert QImage region to grayscale
    static QImage toGrayscale(const QImage &image);

    TrackingResult m_result;
    int m_searchMargin = 50;       // pixel margin around last known position
    double m_minConfidence = 0.5;  // minimum NCC score to accept
};
