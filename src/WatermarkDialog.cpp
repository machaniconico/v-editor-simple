#include "WatermarkDialog.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Helper: generate a gradient sample image for the preview
// ---------------------------------------------------------------------------
static QImage makeSampleImage()
{
    QImage img(320, 180, QImage::Format_ARGB32);
    QPainter p(&img);
    QLinearGradient grad(0, 0, 320, 180);
    grad.setColorAt(0.0, QColor(30,  80, 160));
    grad.setColorAt(0.5, QColor(10, 160, 120));
    grad.setColorAt(1.0, QColor(160, 60,  30));
    p.fillRect(img.rect(), grad);
    p.setPen(Qt::white);
    p.drawText(img.rect(), Qt::AlignCenter, QStringLiteral("Preview"));
    p.end();
    return img;
}

// ---------------------------------------------------------------------------
// WatermarkDialog ctor
// ---------------------------------------------------------------------------
WatermarkDialog::WatermarkDialog(QWidget *parent)
    : QDialog(parent)
{
    setModal(false);
    setWindowTitle(QStringLiteral("Watermark Overlay"));
    m_sample = makeSampleImage();
    buildUi();
    updatePreview();
}

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void WatermarkDialog::buildUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // --- Mode ---
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(QStringLiteral("Text"),  0);
    m_modeCombo->addItem(QStringLiteral("Image"), 1);
    form->addRow(QStringLiteral("Mode:"), m_modeCombo);

    // --- Image path ---
    auto *pathRow = new QHBoxLayout();
    m_imagePathEdit = new QLineEdit(this);
    m_imagePathEdit->setPlaceholderText(QStringLiteral("Image file path..."));
    m_browseBtn = new QPushButton(QStringLiteral("Browse..."), this);
    pathRow->addWidget(m_imagePathEdit);
    pathRow->addWidget(m_browseBtn);
    form->addRow(QStringLiteral("Image:"), pathRow);

    // --- Text ---
    m_textEdit = new QLineEdit(this);
    m_textEdit->setText(m_cfg.text);
    form->addRow(QStringLiteral("Text:"), m_textEdit);

    mainLayout->addLayout(form);

    // --- Position grid (3 columns x 2 rows) ---
    auto *posBox = new QGroupBox(QStringLiteral("Position"), this);
    auto *posGrid = new QGridLayout(posBox);
    posGrid->setSpacing(4);

    m_posGroup = new QButtonGroup(this);

    struct PosEntry { const char *label; watermark::Position pos; int row; int col; };
    const PosEntry entries[] = {
        { "Top Left",     watermark::Position::TopLeft,     0, 0 },
        { "Top Right",    watermark::Position::TopRight,    0, 1 },
        { "Center",       watermark::Position::Center,      0, 2 },
        { "Bottom Left",  watermark::Position::BottomLeft,  1, 0 },
        { "Bottom Right", watermark::Position::BottomRight, 1, 1 },
        { "Tiled",        watermark::Position::Tiled,       1, 2 },
    };

    for (const auto &e : entries) {
        auto *rb = new QRadioButton(QLatin1String(e.label), posBox);
        m_posGroup->addButton(rb, static_cast<int>(e.pos));
        posGrid->addWidget(rb, e.row, e.col);
        if (e.pos == m_cfg.position) {
            rb->setChecked(true);
        }
    }

    mainLayout->addWidget(posBox);

    // --- Sliders ---
    auto *sliderForm = new QFormLayout();

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(0, 100);
    m_opacitySlider->setValue(static_cast<int>(m_cfg.opacity * 100.0));
    sliderForm->addRow(QStringLiteral("Opacity (%):"), m_opacitySlider);

    m_scaleSlider = new QSlider(Qt::Horizontal, this);
    m_scaleSlider->setRange(1, 50);
    m_scaleSlider->setValue(static_cast<int>(m_cfg.scale * 100.0));
    sliderForm->addRow(QStringLiteral("Scale (%):"), m_scaleSlider);

    m_rotationSlider = new QSlider(Qt::Horizontal, this);
    m_rotationSlider->setRange(-180, 180);
    m_rotationSlider->setValue(static_cast<int>(m_cfg.rotationDeg));
    sliderForm->addRow(QStringLiteral("Rotation (deg):"), m_rotationSlider);

    mainLayout->addLayout(sliderForm);

    // --- Preview ---
    m_preview = new QLabel(this);
    m_preview->setFixedSize(320, 180);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    mainLayout->addWidget(m_preview, 0, Qt::AlignCenter);

    // --- Batch Apply button ---
    auto *batchBtn = new QPushButton(QStringLiteral("Batch Apply..."), this);
    mainLayout->addWidget(batchBtn, 0, Qt::AlignRight);

    // --- Connections ---
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WatermarkDialog::onModeChanged);
    connect(m_browseBtn, &QPushButton::clicked,
            this, &WatermarkDialog::onBrowseImageClicked);
    connect(m_textEdit, &QLineEdit::textChanged,
            this, [this](const QString &) { onParamChanged(); });
    connect(m_imagePathEdit, &QLineEdit::textChanged,
            this, [this](const QString &) { onParamChanged(); });
    connect(m_posGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int) { onParamChanged(); });
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, [this](int) { onParamChanged(); });
    connect(m_scaleSlider, &QSlider::valueChanged,
            this, [this](int) { onParamChanged(); });
    connect(m_rotationSlider, &QSlider::valueChanged,
            this, [this](int) { onParamChanged(); });
    connect(batchBtn, &QPushButton::clicked,
            this, &WatermarkDialog::onBatchApplyClicked);

    syncModeWidgets();
}

// ---------------------------------------------------------------------------
// syncModeWidgets — show/hide image vs text controls based on mode
// ---------------------------------------------------------------------------
void WatermarkDialog::syncModeWidgets()
{
    const bool isImage = (m_modeCombo->currentIndex() == 1);
    m_imagePathEdit->setEnabled(isImage);
    m_browseBtn->setEnabled(isImage);
    m_textEdit->setEnabled(!isImage);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void WatermarkDialog::onModeChanged(int /*index*/)
{
    syncModeWidgets();
    onParamChanged();
}

void WatermarkDialog::onBrowseImageClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select Watermark Image"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.svg *.webp);;All Files (*)"));
    if (!path.isEmpty()) {
        m_imagePathEdit->setText(path);
    }
}

void WatermarkDialog::onParamChanged()
{
    // Sync cfg from UI
    m_cfg.mode        = (m_modeCombo->currentIndex() == 1)
                        ? watermark::Mode::Image
                        : watermark::Mode::Text;
    m_cfg.imagePath   = m_imagePathEdit->text();
    m_cfg.text        = m_textEdit->text();
    m_cfg.opacity     = m_opacitySlider->value() / 100.0;
    m_cfg.scale       = m_scaleSlider->value()   / 100.0;
    m_cfg.rotationDeg = static_cast<double>(m_rotationSlider->value());

    const int posId = m_posGroup->checkedId();
    if (posId >= 0) {
        m_cfg.position = static_cast<watermark::Position>(posId);
    }

    updatePreview();
}

void WatermarkDialog::onBatchApplyClicked()
{
    const QStringList inputs = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Select Input Images"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp);;All Files (*)"));
    if (inputs.isEmpty()) {
        return;
    }

    const QString outDir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Output Directory"));
    if (outDir.isEmpty()) {
        return;
    }

    const int count = watermark::batchApply(inputs, outDir, m_cfg);
    QMessageBox::information(
        this,
        QStringLiteral("Batch Apply"),
        QStringLiteral("Watermark applied to %1 of %2 image(s).")
            .arg(count)
            .arg(inputs.size()));
}

// ---------------------------------------------------------------------------
// updatePreview
// ---------------------------------------------------------------------------
void WatermarkDialog::updatePreview()
{
    const QImage composited = watermark::applyWatermark(m_sample, m_cfg);
    m_preview->setPixmap(QPixmap::fromImage(composited).scaled(
        m_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
