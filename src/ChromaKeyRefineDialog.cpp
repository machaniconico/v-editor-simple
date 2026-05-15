#include "ChromaKeyRefineDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QGroupBox>

static const int kPreviewSize = 320;

ChromaKeyRefineDialog::ChromaKeyRefineDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Chroma Key Refine"));
    setModal(false);

    // --- Before / After preview row ---
    m_beforeView = new QLabel;
    m_beforeView->setFixedSize(kPreviewSize, kPreviewSize);
    m_beforeView->setAlignment(Qt::AlignCenter);
    m_beforeView->setStyleSheet("background: #1a1a1a; border: 1px solid #444;");
    m_beforeView->setText(tr("Before"));

    m_afterView = new QLabel;
    m_afterView->setFixedSize(kPreviewSize, kPreviewSize);
    m_afterView->setAlignment(Qt::AlignCenter);
    m_afterView->setStyleSheet("background: #1a1a1a; border: 1px solid #444;");
    m_afterView->setText(tr("After"));

    auto *previewRow = new QHBoxLayout;
    previewRow->addWidget(m_beforeView);
    previewRow->addSpacing(8);
    previewRow->addWidget(m_afterView);

    // --- Source browse ---
    m_browseBtn = new QPushButton(tr("Browse Source Image..."));
    connect(m_browseBtn, &QPushButton::clicked, this, &ChromaKeyRefineDialog::onBrowseClicked);

    // --- Key colour picker ---
    m_keyColorBtn = new QPushButton;
    m_keyColorBtn->setFixedWidth(80);
    // show initial colour
    {
        QPixmap px(60, 20);
        px.fill(m_cfg.keyColor);
        m_keyColorBtn->setIcon(QIcon(px));
        m_keyColorBtn->setText(m_cfg.keyColor.name());
    }
    connect(m_keyColorBtn, &QPushButton::clicked, this, &ChromaKeyRefineDialog::onPickKeyColor);

    // --- Sliders ---
    auto makeSlider = [](int initial) -> QSlider * {
        auto *s = new QSlider(Qt::Horizontal);
        s->setRange(0, 100);
        s->setValue(initial);
        return s;
    };

    m_simSlider    = makeSlider(static_cast<int>(m_cfg.similarity    * 100));
    m_smoothSlider = makeSlider(static_cast<int>(m_cfg.smoothness    * 100));
    m_spillSlider  = makeSlider(static_cast<int>(m_cfg.spillSuppress * 100));

    connect(m_simSlider,    &QSlider::valueChanged, this, &ChromaKeyRefineDialog::onParamChanged);
    connect(m_smoothSlider, &QSlider::valueChanged, this, &ChromaKeyRefineDialog::onParamChanged);
    connect(m_spillSlider,  &QSlider::valueChanged, this, &ChromaKeyRefineDialog::onParamChanged);

    // --- Form layout ---
    auto *form = new QFormLayout;
    form->addRow(tr("Key color:"),      m_keyColorBtn);
    form->addRow(tr("Similarity:"),     m_simSlider);
    form->addRow(tr("Smoothness:"),     m_smoothSlider);
    form->addRow(tr("Spill suppress:"), m_spillSlider);

    auto *formBox = new QGroupBox(tr("Parameters"));
    formBox->setLayout(form);

    // --- Close button ---
    auto *closeBtn = new QPushButton(tr("Close"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    // --- Assemble ---
    auto *root = new QVBoxLayout(this);
    root->addWidget(m_browseBtn);
    root->addLayout(previewRow);
    root->addWidget(formBox);
    root->addWidget(closeBtn);
    root->setSizeConstraint(QLayout::SetFixedSize);
}

void ChromaKeyRefineDialog::setSourceImage(const QImage &img)
{
    m_source = img;
    showSourceInLabel(img);
    updateAfterView();
}

// --- Slots ---

void ChromaKeyRefineDialog::onBrowseClicked()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Open Image"),
        QString(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.webp);;All files (*)"));

    if (path.isEmpty())
        return;

    QImage img(path);
    if (img.isNull())
        return;

    setSourceImage(img);
}

void ChromaKeyRefineDialog::onPickKeyColor()
{
    QColor c = QColorDialog::getColor(m_cfg.keyColor, this, tr("Pick Key Color"));
    if (!c.isValid())
        return;

    m_cfg.keyColor = c;

    // Update button appearance
    QPixmap px(60, 20);
    px.fill(c);
    m_keyColorBtn->setIcon(QIcon(px));
    m_keyColorBtn->setText(c.name());

    updateAfterView();
}

void ChromaKeyRefineDialog::onParamChanged()
{
    m_cfg.similarity    = m_simSlider->value()    / 100.0;
    m_cfg.smoothness    = m_smoothSlider->value() / 100.0;
    m_cfg.spillSuppress = m_spillSlider->value()  / 100.0;
    updateAfterView();
}

// --- Private helpers ---

void ChromaKeyRefineDialog::updateAfterView()
{
    if (m_source.isNull()) {
        m_afterView->setText(tr("After"));
        return;
    }

    m_result           = chromakey::refineMatte(m_source, m_cfg);
    QImage composited  = compositeOverBg(m_result);

    m_afterView->setPixmap(
        QPixmap::fromImage(composited).scaled(
            kPreviewSize, kPreviewSize,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ChromaKeyRefineDialog::showSourceInLabel(const QImage &img)
{
    m_beforeView->setPixmap(
        QPixmap::fromImage(img).scaled(
            kPreviewSize, kPreviewSize,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage ChromaKeyRefineDialog::makeCheckerboard(int w, int h, int tileSize) const
{
    QImage cb(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bool even = ((x / tileSize) + (y / tileSize)) % 2 == 0;
            cb.setPixel(x, y, even ? qRgb(200, 200, 200) : qRgb(140, 140, 140));
        }
    }
    return cb;
}

QImage ChromaKeyRefineDialog::compositeOverBg(const QImage &matte) const
{
    // Composite ARGB32 matte over a checkerboard to show transparency
    QImage bg = makeCheckerboard(matte.width(), matte.height());
    QImage out(matte.width(), matte.height(), QImage::Format_RGB32);

    for (int y = 0; y < matte.height(); ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(matte.constScanLine(y));
        const QRgb *bgp = reinterpret_cast<const QRgb *>(bg.constScanLine(y));
        QRgb       *dst = reinterpret_cast<QRgb *>(out.scanLine(y));

        for (int x = 0; x < matte.width(); ++x) {
            double a = qAlpha(src[x]) / 255.0;
            int r = static_cast<int>(qRed(src[x])   * a + qRed(bgp[x])   * (1.0 - a));
            int g = static_cast<int>(qGreen(src[x]) * a + qGreen(bgp[x]) * (1.0 - a));
            int b = static_cast<int>(qBlue(src[x])  * a + qBlue(bgp[x])  * (1.0 - a));
            dst[x] = qRgb(r, g, b);
        }
    }
    return out;
}
