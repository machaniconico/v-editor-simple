#include "NodeGraph.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QQueue>
#include <QSet>
#include <utility>

namespace {

struct RegisteredNodeType {
    QString displayName;
    QVector<NodePort> inputs;
    QVector<NodePort> outputs;
    QVariantMap params;
};

QHash<QString, RegisteredNodeType> &nodeTypeRegistry()
{
    static QHash<QString, RegisteredNodeType> registry = [] {
        QHash<QString, RegisteredNodeType> builtins;
        builtins.insert(QStringLiteral("Output"),
                        RegisteredNodeType{
                            QStringLiteral("Output"),
                            {NodePort{QStringLiteral("Image"), NodeSocketType::Image, true}},
                            {},
                            {}
                        });
        return builtins;
    }();
    return registry;
}

QString socketTypeToString(NodeSocketType type)
{
    switch (type) {
    case NodeSocketType::Image:
        return QStringLiteral("Image");
    case NodeSocketType::Mask:
        return QStringLiteral("Mask");
    case NodeSocketType::Number:
        return QStringLiteral("Number");
    case NodeSocketType::Color:
        return QStringLiteral("Color");
    }

    return QStringLiteral("Image");
}

NodeSocketType socketTypeFromJsonValue(const QJsonValue &value)
{
    if (value.isDouble()) {
        switch (value.toInt()) {
        case 0:
            return NodeSocketType::Image;
        case 1:
            return NodeSocketType::Mask;
        case 2:
            return NodeSocketType::Number;
        case 3:
            return NodeSocketType::Color;
        default:
            break;
        }
    }

    const QString typeName = value.toString();
    if (typeName == QLatin1String("Mask")) {
        return NodeSocketType::Mask;
    }
    if (typeName == QLatin1String("Number")) {
        return NodeSocketType::Number;
    }
    if (typeName == QLatin1String("Color")) {
        return NodeSocketType::Color;
    }
    return NodeSocketType::Image;
}

QJsonObject portToJson(const NodePort &port)
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), port.name);
    json.insert(QStringLiteral("type"), socketTypeToString(port.type));
    json.insert(QStringLiteral("isInput"), port.isInput);
    return json;
}

QVector<NodePort> portsFromJson(const QJsonArray &jsonArray)
{
    QVector<NodePort> ports;
    ports.reserve(jsonArray.size());

    for (const QJsonValue &value : jsonArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject json = value.toObject();
        NodePort port;
        port.name = json.value(QStringLiteral("name")).toString();
        port.type = socketTypeFromJsonValue(json.value(QStringLiteral("type")));
        port.isInput = json.value(QStringLiteral("isInput")).toBool(false);
        ports.append(port);
    }

    return ports;
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QPointF pointFromJson(const QJsonObject &json)
{
    if (json.contains(QStringLiteral("canvasPos")) && json.value(QStringLiteral("canvasPos")).isObject()) {
        const QJsonObject canvasPos = json.value(QStringLiteral("canvasPos")).toObject();
        return QPointF(canvasPos.value(QStringLiteral("x")).toDouble(),
                       canvasPos.value(QStringLiteral("y")).toDouble());
    }

    return QPointF(json.value(QStringLiteral("canvasPosX")).toDouble(),
                   json.value(QStringLiteral("canvasPosY")).toDouble());
}

GraphNode buildNodeFromRegistry(const QString &typeName, int id)
{
    GraphNode node;
    node.id = id;
    node.typeName = typeName;
    node.displayName = typeName;
    node.dirty = true;

    const auto it = nodeTypeRegistry().constFind(typeName);
    if (it != nodeTypeRegistry().constEnd()) {
        node.displayName = it->displayName.isEmpty() ? typeName : it->displayName;
        node.inputs = it->inputs;
        node.outputs = it->outputs;
        node.params = it->params;
    }

    return node;
}

} // namespace

void NodeGraph::registerNodeType(const QString &typeName,
                                 const QVector<NodePort> &inputs,
                                 const QVector<NodePort> &outputs,
                                 const QVariantMap &params,
                                 const QString &displayName)
{
    if (typeName.isEmpty()) {
        return;
    }

    nodeTypeRegistry().insert(typeName, RegisteredNodeType{
                                            displayName.isEmpty() ? typeName : displayName,
                                            inputs,
                                            outputs,
                                            params,
                                        });
}

int NodeGraph::addNode(const QString &typeName)
{
    const int id = m_nextNodeId++;
    nodes.append(buildNodeFromRegistry(typeName, id));
    return id;
}

void NodeGraph::removeNode(int id)
{
    const int nodeIndex = indexOfNode(id);
    if (nodeIndex < 0) {
        return;
    }

    QSet<int> affectedNodeIds;
    QVector<NodeConnection> filteredConnections;
    filteredConnections.reserve(connections.size());

    for (const NodeConnection &connection : std::as_const(connections)) {
        if (connection.fromNodeId == id || connection.toNodeId == id) {
            if (connection.toNodeId != id) {
                affectedNodeIds.insert(connection.toNodeId);
            }
            continue;
        }
        filteredConnections.append(connection);
    }

    nodes.removeAt(nodeIndex);
    connections = filteredConnections;

    for (int affectedNodeId : affectedNodeIds) {
        markDirty(affectedNodeId);
    }
}

bool NodeGraph::connect(int fromNode, int fromPort, int toNode, int toPort)
{
    const GraphNode *fromGraphNode = node(fromNode);
    const GraphNode *toGraphNode = node(toNode);
    if (!fromGraphNode || !toGraphNode) {
        return false;
    }

    if (fromPort < 0 || fromPort >= fromGraphNode->outputs.size()) {
        return false;
    }
    if (toPort < 0 || toPort >= toGraphNode->inputs.size()) {
        return false;
    }

    const NodePort &outputPort = fromGraphNode->outputs.at(fromPort);
    const NodePort &inputPort = toGraphNode->inputs.at(toPort);
    if (outputPort.isInput || !inputPort.isInput) {
        return false;
    }
    if (outputPort.type != inputPort.type) {
        return false;
    }

    const NodeConnection candidate{fromNode, fromPort, toNode, toPort};
    QVector<NodeConnection> candidateConnections;
    candidateConnections.reserve(connections.size() + 1);

    bool alreadyConnected = false;
    for (const NodeConnection &connection : std::as_const(connections)) {
        if (connection.toNodeId == toNode && connection.toPortIndex == toPort) {
            if (connection.fromNodeId == fromNode && connection.fromPortIndex == fromPort) {
                alreadyConnected = true;
            }
            continue;
        }
        candidateConnections.append(connection);
    }

    if (alreadyConnected) {
        return true;
    }

    candidateConnections.append(candidate);
    if (topologicalOrderFor(candidateConnections).isEmpty() && !nodes.isEmpty()) {
        return false;
    }

    connections = candidateConnections;
    markDirty(toNode);
    return true;
}

void NodeGraph::disconnect(int toNode, int toPort)
{
    QVector<NodeConnection> filteredConnections;
    filteredConnections.reserve(connections.size());

    bool removedConnection = false;
    for (const NodeConnection &connection : std::as_const(connections)) {
        if (connection.toNodeId == toNode && connection.toPortIndex == toPort) {
            removedConnection = true;
            continue;
        }
        filteredConnections.append(connection);
    }

    if (!removedConnection) {
        return;
    }

    connections = filteredConnections;
    markDirty(toNode);
}

QVector<int> NodeGraph::topologicalOrder() const
{
    return topologicalOrderFor(connections);
}

void NodeGraph::markDirty(int nodeId)
{
    if (!hasNode(nodeId)) {
        return;
    }

    QQueue<int> pending;
    QSet<int> visited;

    pending.enqueue(nodeId);
    visited.insert(nodeId);

    while (!pending.isEmpty()) {
        const int currentNodeId = pending.dequeue();
        if (GraphNode *graphNode = node(currentNodeId)) {
            graphNode->dirty = true;
        }

        for (const NodeConnection &connection : std::as_const(connections)) {
            if (connection.fromNodeId != currentNodeId) {
                continue;
            }
            if (visited.contains(connection.toNodeId)) {
                continue;
            }

            visited.insert(connection.toNodeId);
            pending.enqueue(connection.toNodeId);
        }
    }
}

GraphNode *NodeGraph::node(int id)
{
    const int nodeIndex = indexOfNode(id);
    return nodeIndex >= 0 ? &nodes[nodeIndex] : nullptr;
}

const GraphNode *NodeGraph::node(int id) const
{
    const int nodeIndex = indexOfNode(id);
    return nodeIndex >= 0 ? &nodes.at(nodeIndex) : nullptr;
}

GraphNode *NodeGraph::outputNode() const
{
    return const_cast<GraphNode *>(findOutputNode());
}

QJsonObject NodeGraph::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("nextNodeId"), m_nextNodeId);

    QJsonArray nodeArray;
    for (const GraphNode &graphNode : nodes) {
        QJsonObject nodeJson;
        nodeJson.insert(QStringLiteral("id"), graphNode.id);
        nodeJson.insert(QStringLiteral("typeName"), graphNode.typeName);
        nodeJson.insert(QStringLiteral("displayName"), graphNode.displayName);
        nodeJson.insert(QStringLiteral("params"), QJsonObject::fromVariantMap(graphNode.params));
        nodeJson.insert(QStringLiteral("dirty"), graphNode.dirty);
        nodeJson.insert(QStringLiteral("canvasPos"), pointToJson(graphNode.canvasPos));

        QJsonArray inputArray;
        for (const NodePort &port : graphNode.inputs) {
            inputArray.append(portToJson(port));
        }
        nodeJson.insert(QStringLiteral("inputs"), inputArray);

        QJsonArray outputArray;
        for (const NodePort &port : graphNode.outputs) {
            outputArray.append(portToJson(port));
        }
        nodeJson.insert(QStringLiteral("outputs"), outputArray);

        nodeArray.append(nodeJson);
    }
    json.insert(QStringLiteral("nodes"), nodeArray);

    QJsonArray connectionArray;
    for (const NodeConnection &connection : connections) {
        QJsonObject connectionJson;
        connectionJson.insert(QStringLiteral("fromNodeId"), connection.fromNodeId);
        connectionJson.insert(QStringLiteral("fromPortIndex"), connection.fromPortIndex);
        connectionJson.insert(QStringLiteral("toNodeId"), connection.toNodeId);
        connectionJson.insert(QStringLiteral("toPortIndex"), connection.toPortIndex);
        connectionArray.append(connectionJson);
    }
    json.insert(QStringLiteral("connections"), connectionArray);

    return json;
}

void NodeGraph::fromJson(const QJsonObject &json)
{
    nodes.clear();
    connections.clear();
    m_nextNodeId = 1;

    QSet<int> seenNodeIds;
    const QJsonArray nodeArray = json.value(QStringLiteral("nodes")).toArray();
    nodes.reserve(nodeArray.size());

    for (const QJsonValue &value : nodeArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject nodeJson = value.toObject();
        const int nodeId = nodeJson.value(QStringLiteral("id")).toInt(-1);
        if (nodeId <= 0 || seenNodeIds.contains(nodeId)) {
            continue;
        }

        GraphNode graphNode = buildNodeFromRegistry(nodeJson.value(QStringLiteral("typeName")).toString(), nodeId);
        graphNode.displayName = nodeJson.value(QStringLiteral("displayName")).toString(graphNode.displayName);
        graphNode.params = nodeJson.value(QStringLiteral("params")).toObject().toVariantMap();
        graphNode.dirty = nodeJson.value(QStringLiteral("dirty")).toBool(graphNode.dirty);
        graphNode.canvasPos = pointFromJson(nodeJson);

        if (nodeJson.contains(QStringLiteral("inputs"))) {
            graphNode.inputs = portsFromJson(nodeJson.value(QStringLiteral("inputs")).toArray());
        }
        if (nodeJson.contains(QStringLiteral("outputs"))) {
            graphNode.outputs = portsFromJson(nodeJson.value(QStringLiteral("outputs")).toArray());
        }

        seenNodeIds.insert(nodeId);
        m_nextNodeId = qMax(m_nextNodeId, nodeId + 1);
        nodes.append(graphNode);
    }

    const QJsonArray connectionArray = json.value(QStringLiteral("connections")).toArray();
    connections.reserve(connectionArray.size());

    for (const QJsonValue &value : connectionArray) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject connectionJson = value.toObject();
        const NodeConnection connection{
            connectionJson.value(QStringLiteral("fromNodeId")).toInt(-1),
            connectionJson.value(QStringLiteral("fromPortIndex")).toInt(-1),
            connectionJson.value(QStringLiteral("toNodeId")).toInt(-1),
            connectionJson.value(QStringLiteral("toPortIndex")).toInt(-1),
        };

        if (isValidConnection(connection)) {
            connections.append(connection);
        }
    }

    const int storedNextNodeId = json.value(QStringLiteral("nextNodeId")).toInt(m_nextNodeId);
    if (storedNextNodeId > 0) {
        m_nextNodeId = qMax(m_nextNodeId, storedNextNodeId);
    }
}

int NodeGraph::indexOfNode(int id) const
{
    for (int index = 0; index < nodes.size(); ++index) {
        if (nodes.at(index).id == id) {
            return index;
        }
    }
    return -1;
}

bool NodeGraph::hasNode(int id) const
{
    return indexOfNode(id) >= 0;
}

bool NodeGraph::isValidConnection(const NodeConnection &connection) const
{
    const GraphNode *fromGraphNode = node(connection.fromNodeId);
    const GraphNode *toGraphNode = node(connection.toNodeId);
    if (!fromGraphNode || !toGraphNode) {
        return false;
    }

    if (connection.fromPortIndex < 0 || connection.fromPortIndex >= fromGraphNode->outputs.size()) {
        return false;
    }
    if (connection.toPortIndex < 0 || connection.toPortIndex >= toGraphNode->inputs.size()) {
        return false;
    }

    const NodePort &outputPort = fromGraphNode->outputs.at(connection.fromPortIndex);
    const NodePort &inputPort = toGraphNode->inputs.at(connection.toPortIndex);
    return !outputPort.isInput
        && inputPort.isInput
        && outputPort.type == inputPort.type;
}

QVector<int> NodeGraph::topologicalOrderFor(const QVector<NodeConnection> &candidateConnections) const
{
    QVector<int> orderedNodeIds;
    orderedNodeIds.reserve(nodes.size());
    if (nodes.isEmpty()) {
        return orderedNodeIds;
    }

    QHash<int, int> inDegreeByNodeId;
    QHash<int, QVector<int>> adjacencyByNodeId;
    for (const GraphNode &graphNode : nodes) {
        inDegreeByNodeId.insert(graphNode.id, 0);
    }

    for (const NodeConnection &connection : candidateConnections) {
        if (!inDegreeByNodeId.contains(connection.fromNodeId) || !inDegreeByNodeId.contains(connection.toNodeId)) {
            continue;
        }
        adjacencyByNodeId[connection.fromNodeId].append(connection.toNodeId);
        inDegreeByNodeId[connection.toNodeId] = inDegreeByNodeId.value(connection.toNodeId) + 1;
    }

    QQueue<int> readyNodes;
    for (const GraphNode &graphNode : nodes) {
        if (inDegreeByNodeId.value(graphNode.id) == 0) {
            readyNodes.enqueue(graphNode.id);
        }
    }

    while (!readyNodes.isEmpty()) {
        const int nodeId = readyNodes.dequeue();
        orderedNodeIds.append(nodeId);

        const QVector<int> successors = adjacencyByNodeId.value(nodeId);
        for (int successorId : successors) {
            const int updatedInDegree = inDegreeByNodeId.value(successorId) - 1;
            inDegreeByNodeId.insert(successorId, updatedInDegree);
            if (updatedInDegree == 0) {
                readyNodes.enqueue(successorId);
            }
        }
    }

    if (orderedNodeIds.size() != nodes.size()) {
        return {};
    }

    return orderedNodeIds;
}

const GraphNode *NodeGraph::findOutputNode() const
{
    for (const GraphNode &graphNode : nodes) {
        if (graphNode.typeName == QLatin1String("Output")) {
            return &graphNode;
        }
    }
    return nullptr;
}
