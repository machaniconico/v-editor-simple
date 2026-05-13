#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>

struct ProjectData;  // forward declare (defined in ProjectFile.h)

class QThread;

class ProjectCollector : public QObject {
    Q_OBJECT
public:
    explicit ProjectCollector(QObject* parent = nullptr);
    ~ProjectCollector();

    // ProjectData is const ref. Creates destDir/media/, copies all referenced
    // media into it, rewrites paths to destDir-relative "media/<basename>",
    // saves the modified copy to destDir/projectFileName. The original
    // ProjectData is never modified.
    void collect(const ProjectData& data,
                 const QString& destDir,
                 const QString& projectFileName = QStringLiteral("project.veditor"));
    void cancel();

    QStringList warnings() const;
    qint64      bytesCopied() const;

signals:
    void progressChanged(int percent);
    void finished(bool ok, const QString& message);

private:
    friend class CollectorThread;
    void doCollect(ProjectData dataCopy, QString destDir, QString projectFileName);

    QStringList   m_warnings;
    qint64        m_bytesCopied = 0;
    std::atomic<bool> m_cancelled{false};
    QThread*      m_thread = nullptr;
};
