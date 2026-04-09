#pragma once

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include "ProjectSettings.h"

struct ExportPreset {
    QString name;
    QString videoCodec;  // libx264, libx265, libsvtav1, libvpx-vp9
    QString audioCodec;  // aac, libopus, libmp3lame
    QString container;   // mp4, mkv, webm
    int videoBitrate;    // kbps
    int audioBitrate;    // kbps
    int maxFileSizeMB;   // 0 = no limit
};

struct ExportConfig {
    QString outputPath;
    QString videoCodec = "libx264";
    QString audioCodec = "aac";
    QString container = "mp4";
    int videoBitrate = 10000;
    int audioBitrate = 192;
    int width = 1920;
    int height = 1080;
    int fps = 30;
    bool useHardwareAccel = false;
    int maxFileSizeMB = 0;

    QString codecDisplayName() const;
};

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(const ProjectConfig &project, QWidget *parent = nullptr);

    ExportConfig config() const { return m_config; }

    static QVector<ExportPreset> presets();

private slots:
    void onPresetChanged(int index);
    void onBrowseOutput();
    void onExport();

private:
    void setupUI();
    void updateSummary();
    QString defaultExtension() const;

    ExportConfig m_config;
    ProjectConfig m_projectConfig;

    QComboBox *m_presetCombo;
    QComboBox *m_videoCodecCombo;
    QComboBox *m_audioCodecCombo;
    QSpinBox *m_videoBitrateSpin;
    QSpinBox *m_audioBitrateSpin;
    QCheckBox *m_hwAccelCheck;
    QLineEdit *m_outputEdit;
    QLabel *m_summaryLabel;
};
