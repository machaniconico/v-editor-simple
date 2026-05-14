#include "MobileExportDialog.h"
#include "MobilePreset.h"
#include "MobileRotate.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
MobileExportDialog::MobileExportDialog(const QSize& sourceSize,
                                       double       measuredLufs,
                                       QWidget*     parent)
    : QDialog(parent)
    , m_sourceSize(sourceSize)
    , m_measuredLufs(measuredLufs)
{
    setWindowFlags(Qt::Window);
    setObjectName(QStringLiteral("mobileExportDialog"));
    setWindowTitle(tr("モバイル向けエクスポート"));
    resize(560, 360);

    setupUI();

    // Initial populate
    onCategoryChanged(0);
}

// ---------------------------------------------------------------------------
// setupUI
// ---------------------------------------------------------------------------
void MobileExportDialog::setupUI()
{
    // --- Category combo ---------------------------------------------------
    m_categoryCombo = new QComboBox(this);
    // Ordered to match the Category enum: iOSPhone=0, iOSTablet=1, AndroidPhone=2,
    // AndroidTablet=3, Generic=4
    m_categoryCombo->addItem(mobile::categoryDisplayName(mobile::Category::iOSPhone),
                             static_cast<int>(mobile::Category::iOSPhone));
    m_categoryCombo->addItem(mobile::categoryDisplayName(mobile::Category::iOSTablet),
                             static_cast<int>(mobile::Category::iOSTablet));
    m_categoryCombo->addItem(mobile::categoryDisplayName(mobile::Category::AndroidPhone),
                             static_cast<int>(mobile::Category::AndroidPhone));
    m_categoryCombo->addItem(mobile::categoryDisplayName(mobile::Category::AndroidTablet),
                             static_cast<int>(mobile::Category::AndroidTablet));
    m_categoryCombo->addItem(mobile::categoryDisplayName(mobile::Category::Generic),
                             static_cast<int>(mobile::Category::Generic));

    // --- Device combo -----------------------------------------------------
    m_deviceCombo = new QComboBox(this);

    // --- Summary label ----------------------------------------------------
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);

    // --- Rotation indicator -----------------------------------------------
    m_rotateLabel = new QLabel(this);
    m_rotateLabel->setWordWrap(true);

    // --- Output path row --------------------------------------------------
    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setPlaceholderText(tr("出力ファイルパス…"));

    m_browseBtn = new QPushButton(tr("参照…"), this);

    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(m_outputEdit);
    pathRow->addWidget(m_browseBtn);

    // --- Export button ----------------------------------------------------
    m_exportBtn = new QPushButton(tr("エクスポート"), this);
    m_exportBtn->setDefault(true);

    // --- Layout -----------------------------------------------------------
    auto* form = new QFormLayout;
    form->addRow(tr("カテゴリ:"),   m_categoryCombo);
    form->addRow(tr("デバイス:"),   m_deviceCombo);
    form->addRow(tr("仕様:"),       m_summaryLabel);
    form->addRow(tr("縦回転:"),     m_rotateLabel);
    form->addRow(tr("出力先:"),     pathRow);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);
    mainLayout->addStretch();
    mainLayout->addWidget(m_exportBtn);

    // --- Signals ----------------------------------------------------------
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MobileExportDialog::onCategoryChanged);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MobileExportDialog::onDeviceChanged);
    connect(m_browseBtn, &QPushButton::clicked,
            this, &MobileExportDialog::onBrowse);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &MobileExportDialog::onExport);
}

// ---------------------------------------------------------------------------
// populateDeviceCombo
// ---------------------------------------------------------------------------
void MobileExportDialog::populateDeviceCombo(mobile::Category category)
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();

    for (const mobile::DeviceProfile& dev : mobile::allDevices()) {
        if (dev.category == category) {
            m_deviceCombo->addItem(dev.displayName, dev.id);
        }
    }

    m_deviceCombo->blockSignals(false);
}

// ---------------------------------------------------------------------------
// updateSummary
// ---------------------------------------------------------------------------
void MobileExportDialog::updateSummary()
{
    if (m_deviceCombo->count() == 0) {
        m_summaryLabel->setText(tr("デバイスなし"));
        m_rotateLabel->setText(QString());
        return;
    }

    const QString id = m_deviceCombo->currentData().toString();
    const mobile::DeviceProfile dev = mobile::deviceById(id);

    // Build summary
    const QString codecLabel = dev.preferredCodec.toUpper();
    m_summaryLabel->setText(
        QString(tr("%1x%2 / %3fps / %4 / %5kbps / HDR:%6"))
            .arg(dev.maxResolution.width())
            .arg(dev.maxResolution.height())
            .arg(dev.maxFrameRate)
            .arg(codecLabel)
            .arg(dev.maxVideoBitrateKbps)
            .arg(dev.supportsHdr ? tr("対応") : tr("非対応")));

    // Rotation indicator
    const mobile::rotate::RotateDecision dec =
        mobile::rotate::computeRotation(m_sourceSize, dev);

    if (dec.needsRotate) {
        m_rotateLabel->setText(
            QString(tr("9:16 縦向きに自動回転 (%1°)")).arg(dec.angleDeg));
    } else {
        m_rotateLabel->setText(tr("回転不要"));
    }
}

// ---------------------------------------------------------------------------
// resolvedConfig
// ---------------------------------------------------------------------------
ExportConfig MobileExportDialog::resolvedConfig() const
{
    ExportConfig cfg;
    if (m_deviceCombo->count() == 0)
        return cfg;

    const QString id = m_deviceCombo->currentData().toString();
    const mobile::DeviceProfile dev = mobile::deviceById(id);

    cfg = mobile::preset::configForDevice(dev, m_sourceSize, m_measuredLufs);
    cfg.outputPath = m_outputEdit->text();
    return cfg;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void MobileExportDialog::onCategoryChanged(int index)
{
    const int catInt = m_categoryCombo->itemData(index).toInt();
    populateDeviceCombo(static_cast<mobile::Category>(catInt));
    updateSummary();
}

void MobileExportDialog::onDeviceChanged(int /*index*/)
{
    updateSummary();
}

void MobileExportDialog::onBrowse()
{
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("出力ファイルを選択"),
        m_outputEdit->text(),
        tr("MP4 ファイル (*.mp4);;すべてのファイル (*)"));

    if (!path.isEmpty())
        m_outputEdit->setText(path);
}

void MobileExportDialog::onExport()
{
    emit exportRequested(resolvedConfig());
}
