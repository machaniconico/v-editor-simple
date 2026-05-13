#include "SceneCutDialog.h"
#include "SceneCutScanner.h"

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <algorithm>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
QString SceneCutDialog::formatTime(qint64 ms)
{
    int h   = static_cast<int>(ms / 3600000);
    int m   = static_cast<int>((ms % 3600000) / 60000);
    int s   = static_cast<int>((ms % 60000) / 1000);
    int f   = static_cast<int>(ms % 1000);
    return QString("%1:%2:%3.%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(f, 3, 10, QChar('0'));
}

// ---------------------------------------------------------------------------
// constructor
// ---------------------------------------------------------------------------
SceneCutDialog::SceneCutDialog(const QString& clipPath, double clipFps, QWidget* parent)
    : QDialog(parent)
    , m_clipPath(clipPath)
    , m_clipFps(clipFps)
{
    setWindowTitle(tr("シーンカット検出"));
    setMinimumWidth(480);

    // --- top row: threshold / min-scene / start / progress ---
    QLabel* threshLabel = new QLabel(tr("閾値:"), this);
    m_thresholdSpin = new QDoubleSpinBox(this);
    m_thresholdSpin->setRange(0.05, 0.95);
    m_thresholdSpin->setSingleStep(0.05);
    m_thresholdSpin->setValue(0.35);
    m_thresholdSpin->setDecimals(2);

    QLabel* minSceneLabel = new QLabel(tr("最小シーン長 (frame):"), this);
    m_minSceneSpin = new QSpinBox(this);
    m_minSceneSpin->setRange(1, 600);
    m_minSceneSpin->setValue(24);

    m_startBtn = new QPushButton(tr("検出開始"), this);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);

    QHBoxLayout* topLayout = new QHBoxLayout;
    topLayout->addWidget(threshLabel);
    topLayout->addWidget(m_thresholdSpin);
    topLayout->addSpacing(8);
    topLayout->addWidget(minSceneLabel);
    topLayout->addWidget(m_minSceneSpin);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_startBtn);
    topLayout->addStretch();

    // --- middle: list ---
    m_cutList = new QListWidget(this);

    // --- bottom row: select all/none, mode radios, apply/close ---
    m_selectAllBtn  = new QPushButton(tr("全選択"), this);
    m_selectNoneBtn = new QPushButton(tr("全解除"), this);

    m_modeMarkersRadio = new QRadioButton(tr("マーカーとして追加"), this);
    m_modeSplitRadio   = new QRadioButton(tr("ここでクリップ分割"), this);
    m_modeMarkersRadio->setChecked(true);

    QButtonGroup* modeGroup = new QButtonGroup(this);
    modeGroup->addButton(m_modeMarkersRadio);
    modeGroup->addButton(m_modeSplitRadio);

    m_applyBtn = new QPushButton(tr("適用"), this);
    m_closeBtn = new QPushButton(tr("閉じる"), this);

    QHBoxLayout* botLayout = new QHBoxLayout;
    botLayout->addWidget(m_selectAllBtn);
    botLayout->addWidget(m_selectNoneBtn);
    botLayout->addSpacing(16);
    botLayout->addWidget(m_modeMarkersRadio);
    botLayout->addWidget(m_modeSplitRadio);
    botLayout->addStretch();
    botLayout->addWidget(m_applyBtn);
    botLayout->addWidget(m_closeBtn);

    // --- main layout ---
    QVBoxLayout* main = new QVBoxLayout(this);
    main->addLayout(topLayout);
    main->addWidget(m_progressBar);
    main->addWidget(m_cutList, 1);
    main->addLayout(botLayout);
    setLayout(main);

    // --- connections ---
    connect(m_startBtn,     &QPushButton::clicked, this, &SceneCutDialog::onStartDetect);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SceneCutDialog::onSelectAll);
    connect(m_selectNoneBtn,&QPushButton::clicked, this, &SceneCutDialog::onSelectNone);
    connect(m_applyBtn,     &QPushButton::clicked, this, &SceneCutDialog::onApply);
    connect(m_closeBtn,     &QPushButton::clicked, this, [this]() {
        m_wasApplied = false;
        reject();
    });
}

SceneCutDialog::~SceneCutDialog()
{
    if (m_scanner) {
        m_scanner->cancel();
        delete m_scanner;  // synchronous: SceneCutScanner::~SceneCutScanner waits for thread
        m_scanner = nullptr;
    }
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------
void SceneCutDialog::onStartDetect()
{
    if (m_scanner) {
        m_scanner->cancel();
        delete m_scanner;  // synchronous join via ~SceneCutScanner
        m_scanner = nullptr;
    }

    m_scanner = new SceneCutScanner(this);
    connect(m_scanner, &SceneCutScanner::progressChanged,
            this, &SceneCutDialog::onScannerProgress);
    connect(m_scanner, &SceneCutScanner::finished,
            this, &SceneCutDialog::onScannerFinished);

    m_progressBar->setValue(0);
    m_cutList->clear();

    m_scanner->scanFile(m_clipPath,
                        m_thresholdSpin->value(),
                        m_minSceneSpin->value());
}

void SceneCutDialog::onScannerProgress(int percent)
{
    m_progressBar->setValue(percent);
}

void SceneCutDialog::onScannerFinished(bool ok, const QString& /*msg*/)
{
    m_progressBar->setValue(100);

    if (!ok)
        return;

    const QVector<int>&    frames = m_scanner->cutFrames();
    const QVector<qint64>& usVec  = m_scanner->cutTimestampsUs();

    const int count = qMin(frames.size(), usVec.size());
    for (int i = 0; i < count; ++i) {
        qint64 ms = usVec[i] / 1000;
        QString text = QString("カット %1: %2 (frame %3)")
                           .arg(i + 1)
                           .arg(formatTime(ms))
                           .arg(frames[i]);
        QListWidgetItem* item = new QListWidgetItem(text, m_cutList);
        item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        item->setCheckState(Qt::Checked);
        // store microseconds in UserRole for retrieval
        item->setData(Qt::UserRole, usVec[i]);
    }
}

void SceneCutDialog::onSelectAll()
{
    for (int i = 0; i < m_cutList->count(); ++i)
        m_cutList->item(i)->setCheckState(Qt::Checked);
}

void SceneCutDialog::onSelectNone()
{
    for (int i = 0; i < m_cutList->count(); ++i)
        m_cutList->item(i)->setCheckState(Qt::Unchecked);
}

void SceneCutDialog::onApply()
{
    m_wasApplied = true;
    accept();
}

// ---------------------------------------------------------------------------
// accessors
// ---------------------------------------------------------------------------
QVector<qint64> SceneCutDialog::acceptedCutTimestampsMs() const
{
    QVector<qint64> result;
    for (int i = 0; i < m_cutList->count(); ++i) {
        QListWidgetItem* item = m_cutList->item(i);
        if (item->checkState() == Qt::Checked) {
            qint64 us = item->data(Qt::UserRole).toLongLong();
            result.append(us / 1000);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

SceneCutDialog::ApplyMode SceneCutDialog::applyMode() const
{
    return m_modeSplitRadio->isChecked() ? ApplyMode::SplitClip : ApplyMode::AddMarkers;
}

bool SceneCutDialog::wasApplied() const
{
    return m_wasApplied;
}
