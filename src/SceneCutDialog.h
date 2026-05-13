#pragma once
#include <QDialog>
#include <QVector>

class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QProgressBar;
class QListWidget;
class QRadioButton;
class SceneCutScanner;

class SceneCutDialog : public QDialog {
    Q_OBJECT
public:
    enum class ApplyMode { AddMarkers, SplitClip };

    explicit SceneCutDialog(const QString& clipPath, double clipFps, QWidget* parent = nullptr);
    ~SceneCutDialog();

    QVector<qint64> acceptedCutTimestampsMs() const;  // チェック済みのみ、昇順
    ApplyMode       applyMode() const;
    bool            wasApplied() const;

private slots:
    void onStartDetect();
    void onSelectAll();
    void onSelectNone();
    void onApply();
    void onScannerProgress(int percent);
    void onScannerFinished(bool ok, const QString& message);

private:
    static QString formatTime(qint64 ms);

    QString m_clipPath;
    double m_clipFps = 0.0;

    QDoubleSpinBox* m_thresholdSpin  = nullptr;
    QSpinBox*       m_minSceneSpin   = nullptr;
    QPushButton*    m_startBtn        = nullptr;
    QProgressBar*   m_progressBar     = nullptr;
    QListWidget*    m_cutList         = nullptr;
    QPushButton*    m_selectAllBtn    = nullptr;
    QPushButton*    m_selectNoneBtn   = nullptr;
    QRadioButton*   m_modeMarkersRadio = nullptr;
    QRadioButton*   m_modeSplitRadio   = nullptr;
    QPushButton*    m_applyBtn         = nullptr;
    QPushButton*    m_closeBtn         = nullptr;

    SceneCutScanner* m_scanner = nullptr;
    bool m_wasApplied = false;
};
