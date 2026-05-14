#include "ColorMatchDialog.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

// ---------------------------------------------------------------------------
// ctor
// ---------------------------------------------------------------------------
ColorMatchDialog::ColorMatchDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Color Match"));
    setWindowFlags(Qt::Window);

    // ---- button row ----
    m_btnReference = new QPushButton(tr("Reference image..."), this);
    m_btnTarget    = new QPushButton(tr("Target image..."),    this);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_btnReference);
    btnRow->addWidget(m_btnTarget);

    // ---- thumbnail row ----
    m_lblRefThumb = new QLabel(this);
    m_lblRefThumb->setFixedSize(200, 150);
    m_lblRefThumb->setAlignment(Qt::AlignCenter);
    m_lblRefThumb->setText(tr("Reference"));
    m_lblRefThumb->setStyleSheet("border: 1px solid gray;");

    m_lblTgtThumb = new QLabel(this);
    m_lblTgtThumb->setFixedSize(200, 150);
    m_lblTgtThumb->setAlignment(Qt::AlignCenter);
    m_lblTgtThumb->setText(tr("Target"));
    m_lblTgtThumb->setStyleSheet("border: 1px solid gray;");

    auto *thumbRow = new QHBoxLayout;
    thumbRow->addWidget(m_lblRefThumb);
    thumbRow->addWidget(m_lblTgtThumb);

    // ---- before / after preview ----
    m_lblBefore = new QLabel(this);
    m_lblBefore->setFixedSize(200, 150);
    m_lblBefore->setAlignment(Qt::AlignCenter);
    m_lblBefore->setText(tr("Before"));
    m_lblBefore->setStyleSheet("border: 1px solid gray;");

    m_lblAfter = new QLabel(this);
    m_lblAfter->setFixedSize(200, 150);
    m_lblAfter->setAlignment(Qt::AlignCenter);
    m_lblAfter->setText(tr("After"));
    m_lblAfter->setStyleSheet("border: 1px solid gray;");

    auto *previewRow = new QHBoxLayout;
    previewRow->addWidget(m_lblBefore);
    previewRow->addWidget(m_lblAfter);

    // ---- bottom controls ----
    auto *lblSize = new QLabel(tr("LUT size:"), this);
    m_cbLutSize   = new QComboBox(this);
    m_cbLutSize->addItem(QStringLiteral("17"),  17);
    m_cbLutSize->addItem(QStringLiteral("33"),  33);
    m_cbLutSize->addItem(QStringLiteral("65"),  65);
    m_cbLutSize->setCurrentIndex(1); // default 33

    m_btnGenerate = new QPushButton(tr("Generate && Export LUT..."), this);
    m_btnGenerate->setEnabled(false);

    auto *ctrlRow = new QHBoxLayout;
    ctrlRow->addWidget(lblSize);
    ctrlRow->addWidget(m_cbLutSize);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_btnGenerate);

    // ---- assemble ----
    auto *root = new QVBoxLayout(this);
    root->addLayout(btnRow);
    root->addLayout(thumbRow);
    root->addLayout(previewRow);
    root->addLayout(ctrlRow);

    // ---- connections ----
    connect(m_btnReference, &QPushButton::clicked, this, &ColorMatchDialog::onSelectReference);
    connect(m_btnTarget,    &QPushButton::clicked, this, &ColorMatchDialog::onSelectTarget);
    connect(m_btnGenerate,  &QPushButton::clicked, this, &ColorMatchDialog::onGenerate);
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------
void ColorMatchDialog::onSelectReference()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Reference Image"), QString(),
        tr("Images (*.png *.jpg *.bmp)"));
    if (path.isEmpty()) return;

    m_refImage = QImage(path);
    if (!m_refImage.isNull()) {
        m_lblRefThumb->setPixmap(
            QPixmap::fromImage(m_refImage).scaled(
                200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    updateGenerateButton();
    updatePreview();
}

void ColorMatchDialog::onSelectTarget()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select Target Image"), QString(),
        tr("Images (*.png *.jpg *.bmp)"));
    if (path.isEmpty()) return;

    m_tgtImage = QImage(path);
    if (!m_tgtImage.isNull()) {
        m_lblTgtThumb->setPixmap(
            QPixmap::fromImage(m_tgtImage).scaled(
                200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    updateGenerateButton();
    updatePreview();
}

void ColorMatchDialog::onGenerate()
{
    if (m_refImage.isNull() || m_tgtImage.isNull()) return;

    const int lutSize = m_cbLutSize->currentData().toInt();

    const colormatch::analyze::ColorStats refStats =
        colormatch::analyze::analyzeImage(m_refImage);
    const colormatch::analyze::ColorStats tgtStats =
        colormatch::analyze::analyzeImage(m_tgtImage);

    // Generate LUT: transforms target stats → reference stats
    const colormatch::lut::Lut3D lut =
        colormatch::lut::generateMatchLut(tgtStats, refStats, lutSize);

    const QString savePath = QFileDialog::getSaveFileName(
        this, tr("Export LUT"), QStringLiteral("ColorMatchLUT.cube"),
        tr("CUBE LUT (*.cube)"));
    if (savePath.isEmpty()) return;

    const bool ok = colormatch::lut::exportCube(lut, savePath);
    if (!ok) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write LUT file:\n%1").arg(savePath));
        return;
    }

    emit lutGenerated(savePath);
}

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
void ColorMatchDialog::updateGenerateButton()
{
    m_btnGenerate->setEnabled(!m_refImage.isNull() && !m_tgtImage.isNull());
}

void ColorMatchDialog::updatePreview()
{
    if (!m_tgtImage.isNull()) {
        m_lblBefore->setPixmap(
            QPixmap::fromImage(m_tgtImage).scaled(
                200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    if (!m_refImage.isNull() && !m_tgtImage.isNull()) {
        const int lutSize = m_cbLutSize->currentData().toInt();
        const colormatch::analyze::ColorStats refStats =
            colormatch::analyze::analyzeImage(m_refImage);
        const colormatch::analyze::ColorStats tgtStats =
            colormatch::analyze::analyzeImage(m_tgtImage);
        const colormatch::lut::Lut3D lut =
            colormatch::lut::generateMatchLut(tgtStats, refStats, lutSize);
        const QImage preview = applyLutToImage(m_tgtImage, lut);
        m_lblAfter->setPixmap(
            QPixmap::fromImage(preview).scaled(
                200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// static
QImage ColorMatchDialog::applyLutToImage(const QImage &img,
                                          const colormatch::lut::Lut3D &lut)
{
    if (img.isNull() || lut.size <= 0 || lut.data.isEmpty()) return img;

    QImage src = img.convertToFormat(QImage::Format_RGB32);
    QImage dst = src.copy();

    const int N = lut.size;
    const int w = src.width();
    const int h = src.height();

    for (int y = 0; y < h; ++y) {
        const QRgb *srcLine = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb       *dstLine = reinterpret_cast<QRgb *>(dst.scanLine(y));

        for (int x = 0; x < w; ++x) {
            const QRgb px = srcLine[x];
            const int ri = qRed(px)   * (N - 1) / 255;
            const int gi = qGreen(px) * (N - 1) / 255;
            const int bi = qBlue(px)  * (N - 1) / 255;

            // Index: R varies fastest (.cube spec)
            const int idx = bi * N * N + gi * N + ri;
            const QVector3D &entry = lut.data.at(idx);

            dstLine[x] = qRgb(
                qBound(0, static_cast<int>(entry.x() * 255.0f + 0.5f), 255),
                qBound(0, static_cast<int>(entry.y() * 255.0f + 0.5f), 255),
                qBound(0, static_cast<int>(entry.z() * 255.0f + 0.5f), 255));
        }
    }
    return dst;
}
