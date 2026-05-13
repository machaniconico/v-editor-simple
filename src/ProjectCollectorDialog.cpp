#include "ProjectCollectorDialog.h"
#include "ProjectCollector.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>

ProjectCollectorDialog::ProjectCollectorDialog(const ProjectData& data, QWidget* parent)
    : QDialog(parent)
    , m_data(data)
{
    setWindowTitle("Collect Files");
    setMinimumWidth(520);

    auto* formLayout = new QFormLayout;

    // Destination folder row
    auto* destRow = new QHBoxLayout;
    m_destEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton("参照...", this);
    destRow->addWidget(m_destEdit);
    destRow->addWidget(m_browseBtn);
    formLayout->addRow("出力先フォルダ:", destRow);

    // Project file name row
    m_projectFileNameEdit = new QLineEdit("project.veditor", this);
    formLayout->addRow("プロジェクトファイル名:", m_projectFileNameEdit);

    // Media count label
    m_mediaCountLabel = new QLabel(
        QString("参照メディア: %1 件").arg(countReferencedMedia()), this);
    formLayout->addRow(m_mediaCountLabel);

    // Collect button
    m_collectBtn = new QPushButton("収集開始", this);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);

    // Log area
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumBlockCount(500);
    m_logEdit->setMinimumHeight(120);

    // Close button
    m_closeBtn = new QPushButton("閉じる", this);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_collectBtn);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_logEdit);
    mainLayout->addWidget(m_closeBtn);
    setLayout(mainLayout);

    connect(m_browseBtn,  &QPushButton::clicked, this, &ProjectCollectorDialog::onBrowseDest);
    connect(m_collectBtn, &QPushButton::clicked, this, &ProjectCollectorDialog::onStartCollect);
    connect(m_closeBtn,   &QPushButton::clicked, this, &QDialog::close);
}

ProjectCollectorDialog::~ProjectCollectorDialog()
{
    if (m_collector)
        m_collector->cancel();
}

void ProjectCollectorDialog::closeEvent(QCloseEvent* event)
{
    if (m_collector)
        m_collector->cancel();
    QDialog::closeEvent(event);
}

bool ProjectCollectorDialog::didCollect() const
{
    return m_didCollect;
}

QString ProjectCollectorDialog::outputProjectPath() const
{
    return m_outputProjectPath;
}

void ProjectCollectorDialog::onBrowseDest()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "出力先フォルダ", m_destEdit->text());
    if (!dir.isEmpty())
        m_destEdit->setText(dir);
}

void ProjectCollectorDialog::onStartCollect()
{
    QString destDir = m_destEdit->text().trimmed();
    if (destDir.isEmpty() || !QDir(destDir).exists()) {
        QMessageBox::warning(this, "エラー", "出力先フォルダを指定してください");
        return;
    }

    QString projectFileName = m_projectFileNameEdit->text().trimmed();
    if (projectFileName.isEmpty())
        projectFileName = "project.veditor";

    if (m_collector) {
        m_collector->cancel();
        m_collector->deleteLater();
        m_collector = nullptr;
    }

    m_collector = new ProjectCollector(this);
    connect(m_collector, &ProjectCollector::progressChanged,
            this, &ProjectCollectorDialog::onCollectorProgress);
    connect(m_collector, &ProjectCollector::finished,
            this, &ProjectCollectorDialog::onCollectorFinished);

    m_collectBtn->setEnabled(false);
    m_browseBtn->setEnabled(false);
    m_closeBtn->setEnabled(false);
    m_destEdit->setEnabled(false);
    m_projectFileNameEdit->setEnabled(false);
    m_progressBar->setValue(0);
    m_logEdit->clear();

    m_collector->collect(m_data, destDir, projectFileName);
}

void ProjectCollectorDialog::onCollectorProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void ProjectCollectorDialog::onCollectorFinished(bool ok, const QString& message)
{
    m_logEdit->appendPlainText(message);
    m_logEdit->appendPlainText(
        QString("--- warnings (%1) ---").arg(m_collector->warnings().size()));
    for (const QString& w : m_collector->warnings())
        m_logEdit->appendPlainText("  • " + w);

    if (ok) {
        m_didCollect = true;
        m_outputProjectPath = m_destEdit->text() + "/" + m_projectFileNameEdit->text();
        m_logEdit->appendPlainText("出力: " + m_outputProjectPath);
    }

    m_collectBtn->setEnabled(true);
    m_browseBtn->setEnabled(true);
    m_closeBtn->setEnabled(true);
    m_destEdit->setEnabled(true);
    m_projectFileNameEdit->setEnabled(true);
}

int ProjectCollectorDialog::countReferencedMedia() const
{
    int count = 0;

    for (const auto& track : m_data.videoTracks)
        for (const auto& clip : track)
            if (!clip.filePath.isEmpty())
                ++count;

    for (const auto& track : m_data.audioTracks)
        for (const auto& clip : track)
            if (!clip.filePath.isEmpty())
                ++count;

    for (const auto& entry : m_data.particleClipEntries)
        if (!entry.clipFilePath.isEmpty())
            ++count;

    return count;
}
