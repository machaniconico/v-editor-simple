#include "LowerThirdDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QSlider>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static QPixmap makeCheckerboard(const QSize &size)
{
    QImage img(size, QImage::Format_RGB32);
    const int tileSize = 16;
    for (int y = 0; y < size.height(); ++y) {
        for (int x = 0; x < size.width(); ++x) {
            const bool dark = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            img.setPixel(x, y, dark ? qRgb(60, 60, 60) : qRgb(90, 90, 90));
        }
    }
    return QPixmap::fromImage(img);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
LowerThirdDialog::LowerThirdDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Lower Third テンプレート"));
    setObjectName(QStringLiteral("lowerThirdDialog"));
    setModal(false);
    resize(900, 480);

    buildUi();

    // Select first style
    if (m_styleList->count() > 0) {
        m_styleList->setCurrentRow(0);
        onStyleChanged(0);
    }
}

// ---------------------------------------------------------------------------
// buildUi
// ---------------------------------------------------------------------------
void LowerThirdDialog::buildUi()
{
    const QVector<lowerthird::LowerThirdStyle> styles = lowerthird::builtInStyles();

    // --- Style list (left panel) ---
    m_styleList = new QListWidget(this);
    m_styleList->setMaximumWidth(160);
    m_styleList->setMinimumWidth(130);
    for (const auto &st : styles) {
        auto *item = new QListWidgetItem(st.name, m_styleList);
        item->setData(Qt::UserRole, st.id);
    }

    // --- Text edits ---
    m_primaryEdit   = new QLineEdit(this);
    m_primaryEdit->setPlaceholderText(tr("Primary text"));

    m_secondaryEdit = new QLineEdit(this);
    m_secondaryEdit->setPlaceholderText(tr("Secondary text"));

    auto *primaryLabel   = new QLabel(tr("Primary:"),   this);
    auto *secondaryLabel = new QLabel(tr("Secondary:"), this);

    auto *textForm = new QVBoxLayout;
    auto *row1 = new QHBoxLayout;
    row1->addWidget(primaryLabel);
    row1->addWidget(m_primaryEdit);
    auto *row2 = new QHBoxLayout;
    row2->addWidget(secondaryLabel);
    row2->addWidget(m_secondaryEdit);
    textForm->addLayout(row1);
    textForm->addLayout(row2);

    // --- Progress slider ---
    auto *progressLabel = new QLabel(tr("Progress:"), this);
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setRange(0, 100);
    m_progressSlider->setValue(100);

    auto *sliderRow = new QHBoxLayout;
    sliderRow->addWidget(progressLabel);
    sliderRow->addWidget(m_progressSlider);

    // --- Preview label ---
    const QSize previewSize(640, 360);
    m_preview = new QLabel(this);
    m_preview->setFixedSize(previewSize);
    m_preview->setAlignment(Qt::AlignCenter);

    // --- Right panel ---
    auto *rightLayout = new QVBoxLayout;
    rightLayout->addLayout(textForm);
    rightLayout->addLayout(sliderRow);
    rightLayout->addWidget(m_preview);
    rightLayout->addStretch();

    // --- Main layout ---
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_styleList);
    mainLayout->addLayout(rightLayout);
    setLayout(mainLayout);

    // --- Connections ---
    connect(m_styleList, &QListWidget::currentRowChanged,
            this, &LowerThirdDialog::onStyleChanged);
    connect(m_primaryEdit, &QLineEdit::textChanged,
            this, &LowerThirdDialog::onTextEdited);
    connect(m_secondaryEdit, &QLineEdit::textChanged,
            this, &LowerThirdDialog::onTextEdited);
    connect(m_progressSlider, &QSlider::valueChanged,
            this, &LowerThirdDialog::onProgressChanged);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void LowerThirdDialog::onStyleChanged(int row)
{
    if (row < 0)
        return;

    const QVector<lowerthird::LowerThirdStyle> styles = lowerthird::builtInStyles();
    if (row >= styles.size())
        return;

    m_current = styles.at(row);

    // Block signals while pre-filling text edits to avoid spurious re-renders
    const bool b1 = m_primaryEdit->blockSignals(true);
    const bool b2 = m_secondaryEdit->blockSignals(true);
    m_primaryEdit->setText(m_current.primaryText);
    m_secondaryEdit->setText(m_current.secondaryText);
    m_primaryEdit->blockSignals(b1);
    m_secondaryEdit->blockSignals(b2);

    updatePreview();
}

void LowerThirdDialog::onTextEdited()
{
    m_current.primaryText   = m_primaryEdit->text();
    m_current.secondaryText = m_secondaryEdit->text();
    updatePreview();
}

void LowerThirdDialog::onProgressChanged(int /*value*/)
{
    updatePreview();
}

// ---------------------------------------------------------------------------
// updatePreview
// ---------------------------------------------------------------------------
void LowerThirdDialog::updatePreview()
{
    const QSize previewSize(640, 360);
    const double progress = m_progressSlider->value() / 100.0;

    // 1. Dark checkerboard background (shows transparency)
    static const QPixmap checker = makeCheckerboard(previewSize);
    QPixmap result = checker;

    // 2. Render the lower-third frame
    QImage frame = lowerthird::renderFrame(m_current, progress, previewSize);

    // 3. Composite frame over checkerboard
    QPainter painter(&result);
    painter.drawImage(0, 0, frame);
    painter.end();

    m_preview->setPixmap(result);
}
