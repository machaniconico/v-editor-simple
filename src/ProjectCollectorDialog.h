#pragma once
#include <QDialog>
#include <QCloseEvent>
#include "ProjectFile.h"  // ProjectData

class QLineEdit;
class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class QLabel;
class ProjectCollector;

class ProjectCollectorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProjectCollectorDialog(const ProjectData& data, QWidget* parent = nullptr);
    ~ProjectCollectorDialog();

    bool didCollect() const;
    QString outputProjectPath() const;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onBrowseDest();
    void onStartCollect();
    void onCollectorProgress(int percent);
    void onCollectorFinished(bool ok, const QString& message);

private:
    int countReferencedMedia() const;

    ProjectData m_data;
    QLineEdit*       m_destEdit = nullptr;
    QPushButton*     m_browseBtn = nullptr;
    QLineEdit*       m_projectFileNameEdit = nullptr;
    QLabel*          m_mediaCountLabel = nullptr;
    QPushButton*     m_collectBtn = nullptr;
    QProgressBar*    m_progressBar = nullptr;
    QPlainTextEdit*  m_logEdit = nullptr;
    QPushButton*     m_closeBtn = nullptr;

    ProjectCollector* m_collector = nullptr;
    bool   m_didCollect = false;
    QString m_outputProjectPath;
};
