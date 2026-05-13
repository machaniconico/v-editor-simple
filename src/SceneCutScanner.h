#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include <atomic>

class QThread;

class SceneCutScanner : public QObject {
    Q_OBJECT
public:
    explicit SceneCutScanner(QObject* parent = nullptr);
    ~SceneCutScanner();

    // Starts scanning in a background QThread. Returns immediately (non-blocking).
    void scanFile(const QString& path, double threshold = 0.35, int minSceneFrames = 24);
    void cancel();

    QVector<int>      cutFrames() const;
    QVector<qint64>   cutTimestampsUs() const;  // microseconds
    double            frameRate() const;
    int               totalFrames() const;

signals:
    void progressChanged(int percent);
    void finished(bool ok, const QString& message);

private:
    void doScan(QString path, double threshold, int minSceneFrames);

    QVector<int>    m_cutFrames;
    QVector<qint64> m_cutTimestampsUs;
    double m_frameRate = 0.0;
    int    m_totalFrames = 0;
    std::atomic<bool> m_cancelled{false};
    QThread* m_thread = nullptr;
};
