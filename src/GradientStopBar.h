#pragma once

#include <QWidget>
#include <QVector>
#include "TextManager.h"

// Illustrator-style gradient stop editor: a horizontal preview bar with
// draggable stop markers below. Click empty area to add, drag to move,
// right-click marker to delete. Always keeps at least 2 stops.
class GradientStopBar : public QWidget {
    Q_OBJECT
public:
    explicit GradientStopBar(QWidget *parent = nullptr);

    void setStops(const QVector<GradientStop> &stops);
    const QVector<GradientStop> &stops() const { return m_stops; }

    int selectedIndex() const { return m_selectedIdx; }
    void setSelectedIndex(int idx);

    // Update a single stop without rebuilding the whole list (used by
    // per-stop color / opacity / position controls in MainWindow).
    void updateStop(int idx, const GradientStop &newStop);

signals:
    void stopsChanged();
    void stopSelected(int index);

protected:
    QSize sizeHint() const override { return QSize(260, 46); }
    QSize minimumSizeHint() const override { return QSize(180, 46); }
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int hitTestMarker(const QPoint &pt) const;
    QRect barRect() const;
    QRect markerRect(int idx) const;
    QColor interpolateColorAt(double t) const;

    QVector<GradientStop> m_stops;
    int m_selectedIdx = -1;
    int m_draggingIdx = -1;
};
