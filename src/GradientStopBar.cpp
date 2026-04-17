#include "GradientStopBar.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <algorithm>

GradientStopBar::GradientStopBar(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(46);
    GradientStop a, b;
    a.position = 0.0; a.color = Qt::white;              a.opacity = 1.0;
    b.position = 1.0; b.color = QColor(255, 200, 0);    b.opacity = 1.0;
    m_stops = { a, b };
    m_selectedIdx = 0;
}

void GradientStopBar::setStops(const QVector<GradientStop> &stops)
{
    m_stops = stops;
    if (m_stops.size() < 2) {
        GradientStop a, b;
        a.position = 0.0; a.color = Qt::white;           a.opacity = 1.0;
        b.position = 1.0; b.color = QColor(255, 200, 0); b.opacity = 1.0;
        m_stops = { a, b };
    }
    std::sort(m_stops.begin(), m_stops.end(),
              [](const GradientStop &a, const GradientStop &b){ return a.position < b.position; });
    m_selectedIdx = qBound(0, m_selectedIdx, m_stops.size() - 1);
    update();
    emit stopsChanged();
    emit stopSelected(m_selectedIdx);
}

void GradientStopBar::setSelectedIndex(int idx)
{
    if (idx < 0 || idx >= m_stops.size() || idx == m_selectedIdx) return;
    m_selectedIdx = idx;
    update();
    emit stopSelected(m_selectedIdx);
}

void GradientStopBar::updateStop(int idx, const GradientStop &newStop)
{
    if (idx < 0 || idx >= m_stops.size()) return;
    m_stops[idx] = newStop;
    // Re-sort if position changed, then find the stop again (identity by
    // matching fields) so selection stays on the edited stop.
    std::sort(m_stops.begin(), m_stops.end(),
              [](const GradientStop &a, const GradientStop &b){ return a.position < b.position; });
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].position == newStop.position
            && m_stops[i].color == newStop.color
            && m_stops[i].opacity == newStop.opacity) {
            m_selectedIdx = i;
            break;
        }
    }
    update();
    emit stopsChanged();
}

QRect GradientStopBar::barRect() const
{
    return QRect(8, 4, qMax(20, width() - 16), 22);
}

QRect GradientStopBar::markerRect(int idx) const
{
    if (idx < 0 || idx >= m_stops.size()) return QRect();
    const QRect bar = barRect();
    const int x = bar.left() + static_cast<int>(m_stops[idx].position * bar.width());
    const int y = bar.bottom() + 2;
    return QRect(x - 6, y, 12, 14);
}

int GradientStopBar::hitTestMarker(const QPoint &pt) const
{
    for (int i = m_stops.size() - 1; i >= 0; --i) {
        if (markerRect(i).contains(pt)) return i;
    }
    return -1;
}

QColor GradientStopBar::interpolateColorAt(double t) const
{
    if (m_stops.isEmpty()) return Qt::white;
    if (t <= m_stops.first().position) return m_stops.first().color;
    if (t >= m_stops.last().position)  return m_stops.last().color;
    for (int i = 1; i < m_stops.size(); ++i) {
        const GradientStop &a = m_stops[i - 1];
        const GradientStop &b = m_stops[i];
        if (t <= b.position) {
            const double span = qMax(1e-6, b.position - a.position);
            const double f = (t - a.position) / span;
            return QColor(
                static_cast<int>(a.color.red()   * (1 - f) + b.color.red()   * f),
                static_cast<int>(a.color.green() * (1 - f) + b.color.green() * f),
                static_cast<int>(a.color.blue()  * (1 - f) + b.color.blue()  * f),
                255);
        }
    }
    return m_stops.last().color;
}

void GradientStopBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect bar = barRect();

    // Checkerboard behind the bar so stop opacity is visible.
    const int tile = 6;
    for (int y = bar.top(); y < bar.bottom(); y += tile) {
        for (int x = bar.left(); x < bar.right(); x += tile) {
            const bool dark = ((x - bar.left()) / tile + (y - bar.top()) / tile) % 2 == 0;
            p.fillRect(QRect(x, y, tile, tile), dark ? QColor(80, 80, 80) : QColor(120, 120, 120));
        }
    }

    // Gradient preview strip.
    QLinearGradient grad(bar.topLeft(), bar.topRight());
    for (const auto &s : m_stops) {
        QColor c = s.color;
        c.setAlphaF(qBound(0.0, s.opacity, 1.0));
        grad.setColorAt(qBound(0.0, s.position, 1.0), c);
    }
    p.fillRect(bar, grad);
    p.setPen(QPen(QColor(30, 30, 30), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(bar);

    // Stop markers: triangle pointing up + square body with the stop color.
    for (int i = 0; i < m_stops.size(); ++i) {
        const QRect m = markerRect(i);
        QPainterPath path;
        path.moveTo(m.center().x(), m.top());
        path.lineTo(m.left(), m.top() + 5);
        path.lineTo(m.left(), m.bottom());
        path.lineTo(m.right(), m.bottom());
        path.lineTo(m.right(), m.top() + 5);
        path.closeSubpath();
        QColor fill = m_stops[i].color;
        fill.setAlpha(255);
        p.setBrush(fill);
        p.setPen(QPen(i == m_selectedIdx ? Qt::white : QColor(30, 30, 30),
                      i == m_selectedIdx ? 2 : 1));
        p.drawPath(path);
    }
}

void GradientStopBar::mousePressEvent(QMouseEvent *event)
{
    const int idx = hitTestMarker(event->pos());
    if (event->button() == Qt::RightButton) {
        if (idx >= 0 && m_stops.size() > 2) {
            m_stops.removeAt(idx);
            m_selectedIdx = qBound(0, idx - 1, m_stops.size() - 1);
            update();
            emit stopsChanged();
            emit stopSelected(m_selectedIdx);
        }
        return;
    }
    if (event->button() != Qt::LeftButton) return;
    if (idx >= 0) {
        m_selectedIdx = idx;
        m_draggingIdx = idx;
        update();
        emit stopSelected(m_selectedIdx);
        return;
    }
    // Empty-area click inside the bar = add a new stop at that position.
    const QRect bar = barRect();
    if (!bar.contains(QPoint(event->pos().x(), bar.center().y()))) return;
    const double t = qBound(0.0,
                            static_cast<double>(event->pos().x() - bar.left()) / qMax(1, bar.width()),
                            1.0);
    GradientStop s;
    s.position = t;
    s.color = interpolateColorAt(t);
    s.opacity = 1.0;
    m_stops.append(s);
    std::sort(m_stops.begin(), m_stops.end(),
              [](const GradientStop &a, const GradientStop &b){ return a.position < b.position; });
    for (int i = 0; i < m_stops.size(); ++i) {
        if (qAbs(m_stops[i].position - t) < 1e-6) { m_selectedIdx = i; break; }
    }
    m_draggingIdx = m_selectedIdx;
    update();
    emit stopsChanged();
    emit stopSelected(m_selectedIdx);
}

void GradientStopBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingIdx < 0 || m_draggingIdx >= m_stops.size()) return;
    const QRect bar = barRect();
    const double t = qBound(0.0,
                            static_cast<double>(event->pos().x() - bar.left()) / qMax(1, bar.width()),
                            1.0);
    m_stops[m_draggingIdx].position = t;
    // Re-sort so adjacent stops don't cross; keep selection pinned by value.
    GradientStop moved = m_stops[m_draggingIdx];
    std::sort(m_stops.begin(), m_stops.end(),
              [](const GradientStop &a, const GradientStop &b){ return a.position < b.position; });
    for (int i = 0; i < m_stops.size(); ++i) {
        if (m_stops[i].position == moved.position
            && m_stops[i].color == moved.color
            && m_stops[i].opacity == moved.opacity) {
            m_draggingIdx = i;
            m_selectedIdx = i;
            break;
        }
    }
    update();
    emit stopsChanged();
    emit stopSelected(m_selectedIdx);
}

void GradientStopBar::mouseReleaseEvent(QMouseEvent *)
{
    m_draggingIdx = -1;
}
