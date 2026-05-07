#include "CurveEditor.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

CurveEditor::CurveEditor(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setMinimumHeight(180);

    QPointF p0(0.0, 0.0), p1(1.0, 1.0);
    m_masterPts = {p0, p1};
    m_rPts      = {p0, p1};
    m_gPts      = {p0, p1};
    m_bPts      = {p0, p1};

    rebuildAllLuts();
}

void CurveEditor::setChannel(Channel ch)
{
    m_channel = ch;
    update();
}

void CurveEditor::setCurveData(const RgbCurveData &data)
{
    m_data = data;
    update();
}

void CurveEditor::resetCurrentChannel()
{
    QVector<QPointF> identity = { QPointF(0.0, 0.0), QPointF(1.0, 1.0) };
    currentPoints() = identity;
    rebuildLut(m_channel);
    emit curvesChanged(m_data);
    update();
}

QVector<QPointF> &CurveEditor::currentPoints()
{
    switch (m_channel) {
    case Red:   return m_rPts;
    case Green: return m_gPts;
    case Blue:  return m_bPts;
    default:    return m_masterPts;
    }
}

const QVector<QPointF> &CurveEditor::currentPoints() const
{
    switch (m_channel) {
    case Red:   return m_rPts;
    case Green: return m_gPts;
    case Blue:  return m_bPts;
    default:    return m_masterPts;
    }
}

QPointF CurveEditor::toWidget(const QPointF &curve) const
{
    float m = kMargin;
    float w = width()  - 2 * m;
    float h = height() - 2 * m;
    return QPointF(m + curve.x() * w, m + (1.0f - (float)curve.y()) * h);
}

QPointF CurveEditor::fromWidget(const QPointF &widget) const
{
    float m = kMargin;
    float w = width()  - 2 * m;
    float h = height() - 2 * m;
    double cx = (widget.x() - m) / w;
    double cy = 1.0 - (widget.y() - m) / h;
    return QPointF(qBound(0.0, cx, 1.0), qBound(0.0, cy, 1.0));
}

int CurveEditor::hitTest(const QPoint &pos) const
{
    const QVector<QPointF> &pts = currentPoints();
    for (int i = 0; i < pts.size(); ++i) {
        QPointF wp = toWidget(pts[i]);
        double dx = wp.x() - pos.x();
        double dy = wp.y() - pos.y();
        if (dx * dx + dy * dy <= (double)(kPtRadius * 2) * (kPtRadius * 2))
            return i;
    }
    return -1;
}

CurveLut CurveEditor::computeLutFromPoints(const QVector<QPointF> &inPts)
{
    CurveLut lut;
    for (int i = 0; i < 256; ++i)
        lut[i] = i / 255.0f;

    if (inPts.size() < 2)
        return lut;

    QVector<QPointF> pts = inPts;
    std::sort(pts.begin(), pts.end(), [](const QPointF &a, const QPointF &b) {
        return a.x() < b.x();
    });

    int n = pts.size();
    int ns = n - 1;

    if (n == 2) {
        double dx = pts[1].x() - pts[0].x();
        if (dx < 1e-10) dx = 1e-10;
        for (int k = 0; k < 256; ++k) {
            double x = k / 255.0;
            double t = (x - pts[0].x()) / dx;
            double y = pts[0].y() + t * (pts[1].y() - pts[0].y());
            lut[k] = (float)qBound(0.0, y, 1.0);
        }
        return lut;
    }

    // Monotone cubic spline (Fritsch-Carlson)
    QVector<double> h(ns), delta(ns);
    for (int i = 0; i < ns; ++i) {
        h[i] = pts[i+1].x() - pts[i].x();
        if (h[i] < 1e-10) h[i] = 1e-10;
        delta[i] = (pts[i+1].y() - pts[i].y()) / h[i];
    }

    QVector<double> m(n, 0.0);
    m[0]  = delta[0];
    m[ns] = delta[ns - 1];
    for (int i = 1; i < ns; ++i)
        m[i] = (delta[i - 1] + delta[i]) * 0.5;

    for (int i = 0; i < ns; ++i) {
        if (std::abs(delta[i]) < 1e-10) {
            m[i]     = 0.0;
            m[i + 1] = 0.0;
        } else {
            double alpha = m[i]     / delta[i];
            double beta  = m[i + 1] / delta[i];
            double r = alpha * alpha + beta * beta;
            if (r > 9.0) {
                double tau = 3.0 / std::sqrt(r);
                m[i]     = tau * alpha * delta[i];
                m[i + 1] = tau * beta  * delta[i];
            }
        }
    }

    for (int k = 0; k < 256; ++k) {
        double x = k / 255.0;

        if (x <= pts[0].x()) {
            lut[k] = (float)qBound(0.0, pts[0].y(), 1.0);
            continue;
        }
        if (x >= pts[ns].x()) {
            lut[k] = (float)qBound(0.0, pts[ns].y(), 1.0);
            continue;
        }

        int seg = 0;
        for (int i = 0; i < ns; ++i) {
            if (x < pts[i + 1].x()) { seg = i; break; }
            seg = i;
        }

        double t  = (x - pts[seg].x()) / h[seg];
        double t2 = t * t, t3 = t2 * t;
        double h00 =  2*t3 - 3*t2 + 1;
        double h10 =    t3 - 2*t2 + t;
        double h01 = -2*t3 + 3*t2;
        double h11 =    t3 -   t2;

        double y = h00 * pts[seg].y()
                 + h10 * h[seg] * m[seg]
                 + h01 * pts[seg + 1].y()
                 + h11 * h[seg] * m[seg + 1];

        lut[k] = (float)qBound(0.0, y, 1.0);
    }

    return lut;
}

void CurveEditor::rebuildLut(Channel ch)
{
    switch (ch) {
    case Red:    m_data.r      = computeLutFromPoints(m_rPts);      break;
    case Green:  m_data.g      = computeLutFromPoints(m_gPts);      break;
    case Blue:   m_data.b      = computeLutFromPoints(m_bPts);      break;
    case Master: m_data.master = computeLutFromPoints(m_masterPts); break;
    }

    auto isIdentity = [](const CurveLut &l) {
        for (int i = 0; i < 256; ++i)
            if (std::abs(l[i] - i / 255.0f) > 1e-4f) return false;
        return true;
    };
    m_data.enabled = !isIdentity(m_data.r) || !isIdentity(m_data.g)
                  || !isIdentity(m_data.b) || !isIdentity(m_data.master);
}

void CurveEditor::rebuildAllLuts()
{
    rebuildLut(Master);
    rebuildLut(Red);
    rebuildLut(Green);
    rebuildLut(Blue);
}

void CurveEditor::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int mg = kMargin;
    QRect drawRect(mg, mg, width() - 2*mg, height() - 2*mg);

    p.fillRect(rect(), QColor("#1a1a1a"));

    // 4x4 grid
    p.setPen(QColor("#333333"));
    for (int i = 1; i < 4; ++i) {
        float x = drawRect.left() + drawRect.width()  * i / 4.0f;
        float y = drawRect.top()  + drawRect.height() * i / 4.0f;
        p.drawLine(QPointF(x, drawRect.top()),  QPointF(x, drawRect.bottom()));
        p.drawLine(QPointF(drawRect.left(), y), QPointF(drawRect.right(), y));
    }

    // Diagonal identity line
    p.setPen(QColor("#404040"));
    p.drawLine(QPointF(drawRect.left(),  drawRect.bottom()),
               QPointF(drawRect.right(), drawRect.top()));

    QColor curveColor;
    const CurveLut *lut = nullptr;
    switch (m_channel) {
    case Red:    curveColor = QColor("#ff4444"); lut = &m_data.r;      break;
    case Green:  curveColor = QColor("#44ff44"); lut = &m_data.g;      break;
    case Blue:   curveColor = QColor("#4488ff"); lut = &m_data.b;      break;
    default:     curveColor = QColor("#cccccc"); lut = &m_data.master; break;
    }

    // Curve polyline through LUT
    QPainterPath curvePath;
    for (int i = 0; i < 256; ++i) {
        QPointF wp = toWidget(QPointF(i / 255.0, (double)(*lut)[i]));
        if (i == 0) curvePath.moveTo(wp);
        else        curvePath.lineTo(wp);
    }
    p.setPen(QPen(curveColor, 1.5));
    p.drawPath(curvePath);

    // Control points
    const QVector<QPointF> &pts = currentPoints();
    for (int i = 0; i < pts.size(); ++i) {
        QPointF wp = toWidget(pts[i]);
        if (i == m_dragIdx) {
            p.setPen(QPen(Qt::white, 2));
            p.setBrush(curveColor.lighter(150));
        } else {
            p.setPen(QPen(Qt::white, 1));
            p.setBrush(curveColor);
        }
        p.drawEllipse(wp, (double)kPtRadius, (double)kPtRadius);
    }
}

void CurveEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    int hit = hitTest(event->pos());
    if (hit >= 0) {
        m_dragIdx = hit;
    } else {
        QPointF newPt = fromWidget(QPointF(event->pos()));
        QVector<QPointF> &pts = currentPoints();
        pts.append(newPt);
        std::sort(pts.begin(), pts.end(), [](const QPointF &a, const QPointF &b) {
            return a.x() < b.x();
        });
        m_dragIdx = -1;
        for (int i = 0; i < pts.size(); ++i) {
            if (pts[i] == newPt) { m_dragIdx = i; break; }
        }
        rebuildLut(m_channel);
        emit curvesChanged(m_data);
    }
    update();
}

void CurveEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIdx < 0) return;

    QVector<QPointF> &pts = currentPoints();
    QPointF newPt = fromWidget(QPointF(event->pos()));

    if (m_dragIdx == 0) {
        newPt.setX(0.0);
    } else if (m_dragIdx == pts.size() - 1) {
        newPt.setX(1.0);
    } else {
        double xMin = pts[m_dragIdx - 1].x() + 0.001;
        double xMax = pts[m_dragIdx + 1].x() - 0.001;
        newPt.setX(qBound(xMin, newPt.x(), xMax));
    }

    pts[m_dragIdx] = newPt;
    rebuildLut(m_channel);
    emit curvesChanged(m_data);
    update();
}

void CurveEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragIdx = -1;
}

void CurveEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;

    int hit = hitTest(event->pos());
    if (hit < 0) return;

    QVector<QPointF> &pts = currentPoints();
    if (hit == 0 || hit == pts.size() - 1) return;

    pts.remove(hit);
    m_dragIdx = -1;
    rebuildLut(m_channel);
    emit curvesChanged(m_data);
    update();
}
