#include "PlanarTrackerDialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QProgressBar>
#include <QPushButton>
#include <QRectF>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtMath>

// ============================================================================
// PlanarCornerWidget
// ============================================================================

PlanarCornerWidget::PlanarCornerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Default corners: unit rectangle until a real image is set
    m_corners.tl = QPointF(0, 0);
    m_corners.tr = QPointF(1, 0);
    m_corners.br = QPointF(1, 1);
    m_corners.bl = QPointF(0, 1);
}

// ---------------------------------------------------------------------------
QRectF PlanarCornerWidget::imageRect() const
{
    if (m_image.isNull())
        return QRectF(0, 0, width(), height());

    const double iw = m_image.width();
    const double ih = m_image.height();
    const double ww = width();
    const double wh = height();

    const double scale = qMin(ww / iw, wh / ih);
    const double dw    = iw * scale;
    const double dh    = ih * scale;
    const double ox    = (ww - dw) * 0.5;
    const double oy    = (wh - dh) * 0.5;
    return QRectF(ox, oy, dw, dh);
}

// ---------------------------------------------------------------------------
QPointF PlanarCornerWidget::imageToWidget(const QPointF& imgPt) const
{
    const QRectF r = imageRect();
    if (m_image.isNull())
        return imgPt;
    return QPointF(r.x() + imgPt.x() * r.width()  / m_image.width(),
                   r.y() + imgPt.y() * r.height() / m_image.height());
}

// ---------------------------------------------------------------------------
QPointF PlanarCornerWidget::widgetToImage(const QPointF& widgetPt) const
{
    const QRectF r = imageRect();
    if (m_image.isNull() || r.width() <= 0 || r.height() <= 0)
        return widgetPt;
    return QPointF((widgetPt.x() - r.x()) * m_image.width()  / r.width(),
                   (widgetPt.y() - r.y()) * m_image.height() / r.height());
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::setReferenceImage(const QImage& image)
{
    m_image = image;
    update();
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::setCorners(const planar::CornerSet& corners)
{
    m_corners = corners;
    update();
}

// ---------------------------------------------------------------------------
planar::CornerSet PlanarCornerWidget::corners() const
{
    return m_corners;
}

// ---------------------------------------------------------------------------
int PlanarCornerWidget::hitTest(const QPointF& widgetPx) const
{
    const QPointF pts[4] = {
        imageToWidget(m_corners.tl),
        imageToWidget(m_corners.tr),
        imageToWidget(m_corners.br),
        imageToWidget(m_corners.bl),
    };
    int    bestIdx  = -1;
    double bestDist = 12.0;   // pixel threshold
    for (int i = 0; i < 4; ++i) {
        const double dx   = widgetPx.x() - pts[i].x();
        const double dy   = widgetPx.y() - pts[i].y();
        const double dist = qSqrt(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx  = i;
        }
    }
    return bestIdx;
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(30, 30, 30));

    // Image
    if (!m_image.isNull()) {
        const QRectF r = imageRect();
        p.drawImage(r, m_image);
    }

    // Quadrilateral outline
    const QPointF wPts[4] = {
        imageToWidget(m_corners.tl),
        imageToWidget(m_corners.tr),
        imageToWidget(m_corners.br),
        imageToWidget(m_corners.bl),
    };
    QPen linePen(QColor(0x40, 0x80, 0xff, 0x88));
    linePen.setWidthF(1.5);
    p.setPen(linePen);
    p.setBrush(Qt::NoBrush);
    {
        QPolygonF poly;
        for (const QPointF& pt : wPts)
            poly << pt;
        poly << wPts[0];   // close
        p.drawPolyline(poly);
    }

    // Corner handles
    const char* labels[4] = { "1", "2", "3", "4" };
    QPen circlePen(QColor(0x40, 0x80, 0xff));
    circlePen.setWidthF(2.0);
    p.setPen(circlePen);
    p.setBrush(QColor(0x40, 0x80, 0xff, 80));

    for (int i = 0; i < 4; ++i) {
        const QPointF& wp = wPts[i];
        p.drawEllipse(wp, 6.0, 6.0);

        // Number label slightly offset to top-right of the circle
        p.setPen(QColor(220, 220, 255));
        QFont f = font();
        f.setPointSize(8);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(wp.x() + 7, wp.y() - 12, 14, 12),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromLatin1(labels[i]));
        p.setPen(circlePen);
    }
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragIndex = hitTest(event->position());
        if (m_dragIndex >= 0)
            update();
    }
    QWidget::mousePressEvent(event);
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragIndex >= 0) {
        const QPointF imgPt = widgetToImage(event->position());
        switch (m_dragIndex) {
        case 0: m_corners.tl = imgPt; break;
        case 1: m_corners.tr = imgPt; break;
        case 2: m_corners.br = imgPt; break;
        case 3: m_corners.bl = imgPt; break;
        default: break;
        }
        emit cornersChanged(m_corners);
        update();
    }
    QWidget::mouseMoveEvent(event);
}

// ---------------------------------------------------------------------------
void PlanarCornerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        m_dragIndex = -1;
    QWidget::mouseReleaseEvent(event);
}

// ============================================================================
// PlanarTrackerDialog
// ============================================================================

PlanarTrackerDialog::PlanarTrackerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("プラナートラッカー"));
    setObjectName(QStringLiteral("planarTrackerDialog"));
    resize(880, 640);

    // --- Corner widget (left ~60%) ---
    m_cornerWidget = new PlanarCornerWidget(this);

    // --- Parameters form (right ~40%) ---
    m_patchSizeSpin = new QSpinBox(this);
    m_patchSizeSpin->setRange(16, 128);
    m_patchSizeSpin->setValue(32);
    m_patchSizeSpin->setSuffix(tr(" px"));

    m_searchRadiusSpin = new QSpinBox(this);
    m_searchRadiusSpin->setRange(4, 64);
    m_searchRadiusSpin->setValue(16);
    m_searchRadiusSpin->setSuffix(tr(" px"));

    m_dampingSpin = new QSpinBox(this);
    m_dampingSpin->setRange(0, 100);
    m_dampingSpin->setValue(30);
    m_dampingSpin->setSuffix(tr(" %"));

    m_resetButton = new QPushButton(tr("リセット"), this);
    m_trackButton = new QPushButton(tr("追跡実行"), this);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setVisible(false);

    m_summaryLabel = new QLabel(QStringLiteral("--"), this);
    m_summaryLabel->setWordWrap(true);

    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    // --- Right panel layout ---
    auto* form = new QFormLayout;
    form->addRow(tr("パッチサイズ:"),    m_patchSizeSpin);
    form->addRow(tr("探索半径:"),        m_searchRadiusSpin);
    form->addRow(tr("ダンピング:"),      m_dampingSpin);

    auto* rightLayout = new QVBoxLayout;
    rightLayout->addLayout(form);
    rightLayout->addWidget(m_resetButton);
    rightLayout->addWidget(m_trackButton);
    rightLayout->addWidget(m_progress);
    rightLayout->addWidget(m_summaryLabel);
    rightLayout->addStretch();
    rightLayout->addWidget(m_buttonBox);

    // --- Main horizontal layout ---
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(m_cornerWidget, 6);   // ~60%
    mainLayout->addLayout(rightLayout,    4);   // ~40%

    // --- Signal connections ---
    connect(m_cornerWidget, &PlanarCornerWidget::cornersChanged,
            this, [this](const planar::CornerSet& c) {
                m_corners = c;
                rebuildSummary();
            });

    connect(m_resetButton, &QPushButton::clicked,
            this, &PlanarTrackerDialog::onResetCorners);
    connect(m_trackButton, &QPushButton::clicked,
            this, &PlanarTrackerDialog::onTrackClicked);

    connect(m_patchSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlanarTrackerDialog::onPatchSizeChanged);
    connect(m_searchRadiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlanarTrackerDialog::onSearchRadiusChanged);
    connect(m_dampingSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlanarTrackerDialog::onDampingChanged);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Sync params with initial spin values
    m_params.patchSizePx    = m_patchSizeSpin->value();
    m_params.searchRadiusPx = m_searchRadiusSpin->value();
    m_params.dampingFactor  = m_dampingSpin->value() / 100.0;

    rebuildSummary();
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::setReferenceFrame(const QImage& frame)
{
    m_reference = frame;
    m_cornerWidget->setReferenceImage(frame);
    onResetCorners();   // reset corners to new image bounds
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::setFrames(const QList<QImage>& frames)
{
    m_frames = frames;
    if (!frames.isEmpty() && m_reference.isNull())
        setReferenceFrame(frames.first());
    rebuildSummary();
}

// ---------------------------------------------------------------------------
planar::CornerSet PlanarTrackerDialog::currentCorners() const
{
    return m_corners;
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::setCorners(const planar::CornerSet& corners)
{
    m_corners = corners;
    m_cornerWidget->setCorners(corners);
}

// ---------------------------------------------------------------------------
QList<planar::Frame> PlanarTrackerDialog::trackResult() const
{
    return m_result;
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::onResetCorners()
{
    const double w = m_reference.isNull() ? 640.0 : m_reference.width();
    const double h = m_reference.isNull() ? 360.0 : m_reference.height();
    m_corners = planar::CornerSet::rectangle(
        QRectF(w * 0.1, h * 0.1, w * 0.8, h * 0.8));
    m_cornerWidget->setCorners(m_corners);
    rebuildSummary();
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::onTrackClicked()
{
    if (m_frames.isEmpty()) {
        QMessageBox::information(this, tr("情報"),
                                 tr("フレームが投入されていません。"));
        return;
    }

    // Sync params from UI
    m_params.patchSizePx    = m_patchSizeSpin->value();
    m_params.searchRadiusPx = m_searchRadiusSpin->value();
    m_params.dampingFactor  = m_dampingSpin->value() / 100.0;

    planar::Tracker tracker;
    tracker.setParams(m_params);

    m_result.clear();
    m_progress->setRange(0, m_frames.size());
    m_progress->setValue(0);
    m_progress->setVisible(true);

    // First frame is the reference
    tracker.setReferenceFrame(m_frames.first(), m_corners);

    for (int i = 1; i < m_frames.size(); ++i) {
        const qint64 timeMs = static_cast<qint64>(i) * 33LL;   // ~30 fps
        planar::Frame f = tracker.trackNextFrame(m_frames[i], i, timeMs);
        m_result.append(f);
        m_progress->setValue(i);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    m_progress->setVisible(false);
    rebuildSummary();
    emit trackComputed(m_result);
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::onPatchSizeChanged(int value)
{
    m_params.patchSizePx = static_cast<double>(value);
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::onSearchRadiusChanged(int value)
{
    m_params.searchRadiusPx = static_cast<double>(value);
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::onDampingChanged(int value)
{
    m_params.dampingFactor = value / 100.0;
}

// ---------------------------------------------------------------------------
void PlanarTrackerDialog::rebuildSummary()
{
    const int total   = m_frames.size();
    const int tracked = m_result.size();

    double avgConf = 0.0;
    for (const planar::Frame& f : m_result)
        avgConf += f.confidence;
    if (tracked > 0)
        avgConf /= tracked;

    const QString text = tr("投入: %1 / 追跡済: %2 / 平均信頼度: %3")
                             .arg(total)
                             .arg(tracked)
                             .arg(avgConf, 0, 'f', 2);
    m_summaryLabel->setText(text);
}
