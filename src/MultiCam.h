#pragma once

#include <QObject>
#include <QVector>
#include <QString>
#include <QImage>

struct CameraSource {
    QString filePath;
    QString label;        // "Camera 1", "Camera 2", etc.
    double syncOffset = 0.0; // seconds offset for sync (positive = delayed)
    double duration = 0.0;
    bool isActive = true;
};

struct CameraCut {
    int cameraIndex;      // which camera is active
    double startTime;     // timeline time
    double endTime;
};

class MultiCamSession : public QObject
{
    Q_OBJECT

public:
    explicit MultiCamSession(QObject *parent = nullptr);

    // Source management
    void addSource(const QString &filePath, const QString &label = QString());
    void removeSource(int index);
    int sourceCount() const { return m_sources.size(); }
    const QVector<CameraSource> &sources() const { return m_sources; }

    // Sync
    void setSyncOffset(int sourceIndex, double offset);
    void autoSyncByAudio(); // cross-correlate audio for auto-sync

    // Cutting
    void switchToCamera(int cameraIndex, double time);
    void addCut(int cameraIndex, double startTime, double endTime);
    void removeCut(int index);
    const QVector<CameraCut> &cuts() const { return m_cuts; }

    // Get active camera at a given time
    int activeCameraAt(double time) const;

    // Multi-view grid layout
    int gridColumns() const;
    int gridRows() const;

    // Generate final edit list (for export)
    struct EditSegment {
        int cameraIndex;
        double sourceStart; // accounting for sync offset
        double sourceEnd;
        double timelineStart;
        double timelineEnd;
    };
    QVector<EditSegment> generateEditList() const;

    // Total duration (max of all sources considering offsets)
    double totalDuration() const;

signals:
    void sourcesChanged();
    void cutsChanged();
    void syncCompleted();

private:
    void sortCuts();

    QVector<CameraSource> m_sources;
    QVector<CameraCut> m_cuts;
};

// Dialog for multi-cam setup
class MultiCamDialog;
