#pragma once

#include "NodeGraph.h"

#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsView>
#include <QWidget>

class QGraphicsScene;
class QKeyEvent;
class QMenu;

namespace {
constexpr int kPortRadius = 6;
constexpr int kPortSpacing = 22;
constexpr int kNodeWidth = 180;
constexpr int kTitleHeight = 24;
constexpr int kHeaderPad = 8;
constexpr int kPortPadX = 14;
constexpr int kBodyPad = 6;
}

class NodeItem : public QGraphicsItem
{
public:
    enum { Type = QGraphicsItem::UserType + 1 };

    explicit NodeItem(GraphNode *node, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    void updateGeometry();

    QPointF portScenePosition(int portIndex, bool isInput) const;

    int hitTestPort(QPointF scenePos, bool isInput) const;

    GraphNode *node() const { return m_node; }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    GraphNode *m_node;
    QRectF m_bodyRect;
    int m_maxPorts;
};

class ConnectionItem : public QGraphicsPathItem
{
public:
    enum { Type = QGraphicsItem::UserType + 2 };

    explicit ConnectionItem(QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }

    void setEndpoints(QPointF outputPos, QPointF inputPos);

    int fromNodeId() const { return m_fromNodeId; }
    int fromPortIndex() const { return m_fromPortIndex; }
    int toNodeId() const { return m_toNodeId; }
    int toPortIndex() const { return m_toPortIndex; }

    void setConnectionInfo(int fromNode, int fromPort, int toNode, int toPort);

private:
    int m_fromNodeId = -1;
    int m_fromPortIndex = -1;
    int m_toNodeId = -1;
    int m_toPortIndex = -1;
};

class NodeCanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit NodeCanvasView(QWidget *parent = nullptr);

signals:
    void canvasContextMenuRequested(QPointF scenePos, QPoint globalPos);
    void canvasClicked();
    void portDragStarted(QPointF outputPortScenePos);
    void portDragMoved(QPointF scenePos);
    void portDragEnded(QPointF scenePos);
    void deleteRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    bool m_portDragging = false;
    QPointF m_dragStartPortPos;
};

class NodeCanvasWidget : public QWidget
{
    Q_OBJECT

public:
    explicit NodeCanvasWidget(QWidget *parent = nullptr);

    void setGraph(NodeGraph *graph);
    NodeGraph *graph() const { return m_graph; }

    void rebuildScene();

signals:
    void graphChanged();
    void nodeSelected(int nodeId);

private slots:
    void onAddNode(const QString &typeName);
    void onDeleteSelected();
    void onPortDragStarted(QPointF outputPortScenePos);
    void onPortDragMoved(QPointF scenePos);
    void onPortDragEnded(QPointF scenePos);
    void onCanvasContextMenuRequested(QPointF scenePos, QPoint globalPos);
    void onCanvasClicked();
    void onSelectionChanged();

private:
    NodeItem *findNodeItem(int nodeId) const;
    ConnectionItem *findConnectionItem(int fromNode, int fromPort, int toNode, int toPort) const;
    void removeConnectionItem(ConnectionItem *item);
    void buildContextMenu(QMenu *menu, QPointF scenePos);

    NodeGraph *m_graph = nullptr;
    NodeCanvasView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QHash<int, NodeItem *> m_nodeItems;
    ConnectionItem *m_tempConnection = nullptr;
    QPointF m_dragStartPortPos;
};
