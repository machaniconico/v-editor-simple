#pragma once

#include <QDialog>
#include <QImage>
#include <QList>
#include <QPointF>
#include <QWidget>

#include "PlanarTracker.h"

class QDialogButtonBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;

// ---------------------------------------------------------------------------
// PlanarCornerWidget — reference image display with 4 draggable corner pins
// ---------------------------------------------------------------------------
class PlanarCornerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PlanarCornerWidget(QWidget* parent = nullptr);

    void setReferenceImage(const QImage& image);
    void setCorners(const planar::CornerSet& corners);
    planar::CornerSet corners() const;

signals:
    void cornersChanged(const planar::CornerSet& corners);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    // Returns index 0..3 (tl/tr/br/bl) if widgetPx is within 12 px of a corner, else -1
    int     hitTest(const QPointF& widgetPx) const;
    QPointF imageToWidget(const QPointF& imgPt) const;
    QPointF widgetToImage(const QPointF& widgetPt) const;

    // Computes the image rect inside the widget (letterbox / aspect-preserving)
    QRectF  imageRect() const;

    QImage          m_image;
    planar::CornerSet m_corners;
    int             m_dragIndex = -1;
};

// ---------------------------------------------------------------------------
// PlanarTrackerDialog — UI dialog for planar (homography) corner tracking
// ---------------------------------------------------------------------------
class PlanarTrackerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PlanarTrackerDialog(QWidget* parent = nullptr);

    // Set the reference frame (first frame / anchor image)
    void setReferenceFrame(const QImage& frame);

    // Supply the sequence of frames to track against
    void setFrames(const QList<QImage>& frames);

    // Corner pin accessors (image-coordinate space)
    planar::CornerSet currentCorners() const;
    void setCorners(const planar::CornerSet& corners);

    // Result after tracking
    QList<planar::Frame> trackResult() const;

signals:
    void trackComputed(const QList<planar::Frame>& frames);

private slots:
    void onResetCorners();
    void onTrackClicked();
    void onPatchSizeChanged(int value);
    void onSearchRadiusChanged(int value);
    void onDampingChanged(int value);

private:
    void rebuildSummary();

    QImage               m_reference;
    QList<QImage>        m_frames;
    planar::CornerSet    m_corners;
    QList<planar::Frame> m_result;
    planar::TrackParams  m_params;

    PlanarCornerWidget* m_cornerWidget      = nullptr;
    QSpinBox*           m_patchSizeSpin     = nullptr;  // px, 16..128
    QSpinBox*           m_searchRadiusSpin  = nullptr;  // px, 4..64
    QSpinBox*           m_dampingSpin       = nullptr;  // %, 0..100
    QPushButton*        m_resetButton       = nullptr;
    QPushButton*        m_trackButton       = nullptr;
    QProgressBar*       m_progress          = nullptr;
    QLabel*             m_summaryLabel      = nullptr;
    QDialogButtonBox*   m_buttonBox         = nullptr;
};
