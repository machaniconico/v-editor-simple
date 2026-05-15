#include "AnimatedExportDialog.h"
#include "AnimatedExport.h"

#include <QImage>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

AnimatedExportDialog::AnimatedExportDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Animated Export (GIF / WebP)"));
    setModal(false);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form       = new QFormLayout();

    // Input folder picker.
    auto *folderRow = new QHBoxLayout();
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(
        tr("Folder of sequentially-named *.png / *.jpg images"));
    auto *browseBtn = new QPushButton(tr("Browse..."), this);
    folderRow->addWidget(m_folderEdit);
    folderRow->addWidget(browseBtn);
    form->addRow(tr("Input folder:"), folderRow);

    // Format.
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(QStringLiteral("GIF"));
    m_formatCombo->addItem(QStringLiteral("WebP"));
    m_formatCombo->setCurrentIndex(1); // default WebP (matches ExportConfig)
    form->addRow(tr("Format:"), m_formatCombo);

    // FPS.
    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(1, 60);
    m_fpsSpin->setValue(15);
    form->addRow(tr("FPS:"), m_fpsSpin);

    // Width.
    m_widthSpin = new QSpinBox(this);
    m_widthSpin->setRange(16, 3840);
    m_widthSpin->setValue(480);
    m_widthSpin->setSuffix(QStringLiteral(" px"));
    form->addRow(tr("Width:"), m_widthSpin);

    mainLayout->addLayout(form);

    m_exportBtn = new QPushButton(tr("Export..."), this);
    mainLayout->addWidget(m_exportBtn);

    connect(browseBtn, &QPushButton::clicked,
            this, &AnimatedExportDialog::onBrowseFolderClicked);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &AnimatedExportDialog::onExportClicked);
}

void AnimatedExportDialog::onBrowseFolderClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Image Sequence Folder"), m_folderEdit->text());
    if (!dir.isEmpty())
        m_folderEdit->setText(dir);
}

void AnimatedExportDialog::onExportClicked()
{
    const QString folder = m_folderEdit->text().trimmed();
    if (folder.isEmpty()) {
        QMessageBox::warning(this, tr("Animated Export"),
                             tr("Please choose an input folder."));
        return;
    }

    QDir dir(folder);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Animated Export"),
                             tr("The selected folder does not exist."));
        return;
    }

    QStringList filters;
    filters << QStringLiteral("*.png") << QStringLiteral("*.jpg")
            << QStringLiteral("*.jpeg");
    QStringList names =
        dir.entryList(filters, QDir::Files, QDir::Name);
    if (names.isEmpty()) {
        QMessageBox::warning(this, tr("Animated Export"),
                             tr("No *.png / *.jpg images found in folder."));
        return;
    }

    QVector<QImage> frames;
    frames.reserve(names.size());
    for (const QString &name : names) {
        QImage img(dir.absoluteFilePath(name));
        if (!img.isNull())
            frames.append(img);
    }
    if (frames.isEmpty()) {
        QMessageBox::warning(this, tr("Animated Export"),
                             tr("Failed to load any images from folder."));
        return;
    }

    const bool isGif = (m_formatCombo->currentIndex() == 0);
    const QString filter = isGif ? tr("GIF Image (*.gif)")
                                 : tr("WebP Image (*.webp)");
    const QString defExt = isGif ? QStringLiteral(".gif")
                                 : QStringLiteral(".webp");

    QString outPath = QFileDialog::getSaveFileName(
        this, tr("Save Animated Image"),
        dir.absoluteFilePath(QStringLiteral("export") + defExt), filter);
    if (outPath.isEmpty())
        return;
    if (!outPath.endsWith(defExt, Qt::CaseInsensitive))
        outPath += defExt;

    animexport::ExportConfig cfg;
    cfg.format = isGif ? animexport::Format::Gif : animexport::Format::WebP;
    cfg.fps    = m_fpsSpin->value();
    cfg.width  = m_widthSpin->value();
    cfg.loop   = 0;

    const bool ok = animexport::exportFrames(frames, outPath, cfg);
    if (ok) {
        QMessageBox::information(
            this, tr("Animated Export"),
            tr("Exported %1 frames to:\n%2")
                .arg(frames.size())
                .arg(outPath));
    } else {
        QMessageBox::critical(
            this, tr("Animated Export"),
            tr("Export failed. The image format may be unsupported "
               "by this build."));
    }
}
