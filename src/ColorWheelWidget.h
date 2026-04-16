#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

class ColorWheelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ColorWheelWidget(const QString &label = QString(), QWidget *parent = nullptr);

    void setColor(double r, double g, double b);
    double redValue() const { return m_r; }
    double greenValue() const { return m_g; }
    double blueValue() const { return m_b; }

signals:
    void colorChanged(double r, double g, double b);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return QSize(120, 140); }
    QSize minimumSizeHint() const override { return QSize(80, 100); }

private:
    void updateFromPos(const QPointF &pos);
    QPointF colorToPos() const;
    QRectF wheelRect() const;
    double wheelRadius() const;

    QString m_label;
    double m_r = 0.0;  // -1.0 to 1.0
    double m_g = 0.0;
    double m_b = 0.0;
    bool m_dragging = false;
};
