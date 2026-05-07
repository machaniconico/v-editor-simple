#pragma once
#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QColor>
#include <array>

using CurveLut = std::array<float, 256>;

struct RgbCurveData {
    CurveLut master;
    CurveLut r;
    CurveLut g;
    CurveLut b;
    bool enabled = false;
    RgbCurveData() {
        for (int i = 0; i < 256; ++i) {
            float v = i / 255.0f;
            master[i] = r[i] = g[i] = b[i] = v;
        }
    }
};

class CurveEditor : public QWidget {
    Q_OBJECT
public:
    enum Channel { Master = 0, Red = 1, Green = 2, Blue = 3 };
    explicit CurveEditor(QWidget *parent = nullptr);
    void setChannel(Channel ch);
    Channel channel() const { return m_channel; }
    RgbCurveData curveData() const { return m_data; }
    void setCurveData(const RgbCurveData &data);
    void resetCurrentChannel();
    QSize sizeHint() const override { return QSize(256, 200); }
signals:
    void curvesChanged(const RgbCurveData &data);
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
private:
    QVector<QPointF> &currentPoints();
    const QVector<QPointF> &currentPoints() const;
    void rebuildLut(Channel ch);
    void rebuildAllLuts();
    static CurveLut computeLutFromPoints(const QVector<QPointF> &pts);
    QPointF toWidget(const QPointF &curve) const;
    QPointF fromWidget(const QPointF &widget) const;
    int hitTest(const QPoint &pos) const;
    Channel m_channel = Master;
    RgbCurveData m_data;
    QVector<QPointF> m_masterPts, m_rPts, m_gPts, m_bPts;
    int m_dragIdx = -1;
    static constexpr int kPtRadius = 5;
    static constexpr int kMargin = 10;
};
