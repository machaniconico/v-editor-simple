#pragma once

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVariantMap>
#include <QVector>

enum class NodeSocketType {
    Image,
    Mask,
    Number,
    Color
};

struct NodePort {
    QString name;
    NodeSocketType type = NodeSocketType::Image;
    bool isInput = false;
};

struct NodeConnection {
    int fromNodeId = -1;
    int fromPortIndex = -1;
    int toNodeId = -1;
    int toPortIndex = -1;
};

class GraphNode
{
public:
    int id = -1;
    QString typeName;
    QString displayName;
    QVector<NodePort> inputs;
    QVector<NodePort> outputs;
    QVariantMap params;
    bool dirty = true;
    QPointF canvasPos;
};

class NodeGraph
{
public:
    static void registerNodeType(const QString &typeName,
                                 const QVector<NodePort> &inputs = {},
                                 const QVector<NodePort> &outputs = {},
                                 const QVariantMap &params = {},
                                 const QString &displayName = QString());

    int addNode(const QString &typeName);
    void removeNode(int id);

    // Destination inputs are single-connection. Connecting a new source to an
    // already-connected input replaces the existing incoming edge.
    bool connect(int fromNode, int fromPort, int toNode, int toPort);
    void disconnect(int toNode, int toPort);

    QVector<int> topologicalOrder() const;
    void markDirty(int nodeId);

    GraphNode *node(int id);
    const GraphNode *node(int id) const;
    GraphNode *outputNode() const;

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &json);

    QVector<GraphNode> nodes;
    QVector<NodeConnection> connections;

private:
    int m_nextNodeId = 1;

    int indexOfNode(int id) const;
    bool hasNode(int id) const;
    bool isValidConnection(const NodeConnection &connection) const;
    QVector<int> topologicalOrderFor(const QVector<NodeConnection> &candidateConnections) const;
    const GraphNode *findOutputNode() const;
};
