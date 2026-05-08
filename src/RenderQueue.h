#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>
#include <QProcess>

enum class RenderJobStatus {
    Pending,
    Rendering,
    Completed,
    Failed,
    Cancelled
};

// Flat-field RenderJob — Premiere Media Encoder / Resolve Deliver style.
// `uuid` is a stable QString id for the spec API (RenderQueueDialog uses it);
// the legacy `id` int field is preserved so existing MainWindow code keeps
// compiling unchanged.
struct RenderJob {
    int id = 0;                  // legacy int id (preserved for MainWindow)
    QString uuid;                // spec API id (QString)
    QString name;
    QString projectFilePath;
    QString outputPath;

    // Preset description + flat encoder params (Render Queue dialog uses these).
    QString preset;              // e.g. "1080p H.264 / 50 Mbps"
    int width = 1920;
    int height = 1080;
    QString codec = "h264";      // "h264" | "hevc" | "av1" | "prores"
    int bitrateBps = 50000000;
    qint64 startUs = 0;          // timeline range start
    qint64 endUs = 0;            // timeline range end (0 = whole timeline)
    int passes = 1;              // 1 or 2 (2-pass VBR)

    // Live state.
    RenderJobStatus status = RenderJobStatus::Pending;  // legacy enum (MainWindow reads this)
    int progressPercent = 0;
    QString error;

    // Legacy fields (still populated for backwards compatibility).
    QJsonObject exportConfig;
    int progress = 0;
    QDateTime startTime;
    QDateTime endTime;
    QString errorMessage;

    // Convenience accessor — returns the spec's lowercase status string.
    QString statusString() const {
        switch (status) {
        case RenderJobStatus::Pending:   return QStringLiteral("pending");
        case RenderJobStatus::Rendering: return QStringLiteral("running");
        case RenderJobStatus::Completed: return QStringLiteral("completed");
        case RenderJobStatus::Failed:    return QStringLiteral("failed");
        case RenderJobStatus::Cancelled: return QStringLiteral("cancelled");
        }
        return QStringLiteral("pending");
    }
};

// Built-in preset descriptor — RenderQueueDialog populates the catalogue
// from RenderQueue::availablePresets().
struct RenderPreset {
    QString name;
    int width;
    int height;
    QString codec;       // "h264" | "hevc" | "av1" | "prores"
    int bitrateBps;
    QString container;   // "mp4" | "mov" | "mkv" | "webm"
};

class RenderQueue : public QObject
{
    Q_OBJECT

public:
    explicit RenderQueue(QObject *parent = nullptr);
    ~RenderQueue();

    // Spec API — preferred for new code (uuid-keyed).
    void addJob(const RenderJob &job);
    void removeJob(const QString &uuid);
    void clear();
    QVector<RenderJob> jobs() const { return m_jobs; }
    bool isRunning() const { return m_running; }
    void start();   // process pending jobs sequentially
    void stop();    // cancel current and stop queue

    static QVector<RenderPreset> availablePresets();
    static RenderJob jobFromPreset(const RenderPreset &preset,
                                   const QString &outputPath,
                                   qint64 startUs = 0,
                                   qint64 endUs = 0);

    // Legacy API — kept so existing callers compile unchanged.
    int addJob(const QString &name, const QString &projectFilePath,
               const QString &outputPath, const QJsonObject &exportConfig);
    void removeJob(int id);
    void clearCompleted();
    void clearAll();

    void startQueue();
    void pauseQueue();
    void resumeQueue();
    void cancelCurrent();

    const RenderJob *currentJob() const;
    int pendingCount() const;
    int completedCount() const;

    void moveJobUp(int id);
    void moveJobDown(int id);
    void moveJobUpUuid(const QString &uuid);
    void moveJobDownUuid(const QString &uuid);

    int totalEstimatedTime() const;

    bool saveQueue(const QString &filePath) const;
    bool loadQueue(const QString &filePath);

signals:
    // Spec signals (QString uuid keyed).
    void jobsChanged();
    void jobProgressUuid(QString uuid, int percent);
    void jobCompletedUuid(QString uuid, bool success, QString error);
    void allCompleted();

    // Legacy signals (still emitted alongside the spec signals).
    void jobStarted(int id);
    void jobProgress(int id, int percent);
    void jobCompleted(int id);
    void jobFailed(int id, const QString &error);
    void queueFinished();
    void queueProgress(int overallPercent);

private:
    void startNextJob();
    QStringList buildFFmpegArgs(const RenderJob &job) const;
    void parseFFmpegOutput(const QString &line);
    int findJobIndex(int id) const;
    int findJobIndexByUuid(const QString &uuid) const;
    static QString mapCodecToFFmpeg(const QString &codec);
    static QString defaultContainerFor(const QString &codec);

    QVector<RenderJob> m_jobs;
    QProcess *m_process = nullptr;
    int m_nextId = 1;
    int m_currentJobIndex = -1;
    bool m_running = false;
    bool m_paused = false;
    double m_currentDuration = 0.0;  // total duration in seconds for progress parsing
};
