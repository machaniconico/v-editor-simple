#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

#include "YoutubeUploadManager.h"

class QPushButton;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QComboBox;
class QTableWidget;

// ---------------------------------------------------------------------------
// YoutubeUploadDialog — Sprint 17 US-YT-4
// モードレスダイアログ。Manager に Job を追加し、ジョブキューの進捗を表示する。
// ---------------------------------------------------------------------------

class YoutubeUploadDialog : public QDialog {
    Q_OBJECT
public:
    explicit YoutubeUploadDialog(youtube::manager::Manager* manager,
                                 QWidget* parent = nullptr);
    ~YoutubeUploadDialog() override = default;

private slots:
    void onBrowseClicked();
    void onUploadClicked();

    // Manager signals
    void onJobAdded(const QString& jobId);
    void onJobProgressChanged(const QString& jobId, int percent);
    void onJobStateChanged(const QString& jobId, youtube::manager::State state);
    void onJobCompleted(const QString& jobId, const QString& videoId);
    void onJobFailed(const QString& jobId, const QString& reason);

private:
    // Returns the display filename for a given jobId row (column 0 text).
    QString stateToString(youtube::manager::State state) const;

    youtube::manager::Manager* m_manager = nullptr;

    // Form widgets
    QPushButton*   m_browseButton   = nullptr;
    QLabel*        m_fileLabel      = nullptr;
    QLineEdit*     m_titleEdit      = nullptr;
    QPlainTextEdit* m_descEdit      = nullptr;
    QComboBox*     m_privacyCombo   = nullptr;
    QLineEdit*     m_tagsEdit       = nullptr;
    QPushButton*   m_uploadButton   = nullptr;

    // Job queue table
    QTableWidget*  m_table          = nullptr;

    // Maps jobId → table row index
    QHash<QString, int> m_jobIdToRow;

    // Currently selected file path
    QString m_selectedFilePath;
};
