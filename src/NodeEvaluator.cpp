#include "NodeEvaluator.h"

#include <QColor>
#include <QHash>
#include <QQueue>
#include <QSet>

#include <QtGlobal>

#include <cmath>
#include <utility>

void NodeEvaluator::setGraph(NodeGraph *graph)
{
    m_graph = graph;
    invalidateAll();
}

void NodeEvaluator::setOutputSize(QSize size)
{
    if (m_outputSize == size) {
        return;
    }

    m_outputSize = std::move(size);
    invalidateAll();
}

void NodeEvaluator::setNodeEvaluatorFn(NodeEvaluatorFn evaluatorFn)
{
    m_nodeEvaluatorFn = std::move(evaluatorFn);
    invalidateAll();
}

QImage NodeEvaluator::render(double time)
{
    if (!m_graph) {
        return transparentImage();
    }

    const GraphNode *outputNode = m_graph->outputNode();
    if (!outputNode) {
        return transparentImage();
    }

    for (const GraphNode &graphNode : m_graph->nodes) {
        if (!graphNode.dirty) {
            continue;
        }
        invalidateCache(graphNode.id);
    }

    const QVector<int> orderedNodeIds = m_graph->topologicalOrder();
    if (orderedNodeIds.isEmpty() && !m_graph->nodes.isEmpty()) {
        return transparentImage();
    }

    QHash<int, QVariant> valuesByNodeId;
    valuesByNodeId.reserve(orderedNodeIds.size());

    for (int nodeId : orderedNodeIds) {
        GraphNode *graphNode = m_graph->node(nodeId);
        if (!graphNode) {
            continue;
        }

        const CacheKey cacheKey = makeCacheKey(nodeId, time);
        const auto cachedIt = m_frameCache.constFind(cacheKey);
        if (cachedIt != m_frameCache.constEnd()) {
            valuesByNodeId.insert(nodeId, cachedIt.value());
            graphNode->dirty = false;
            continue;
        }

        QVector<QVariant> inputs;
        inputs.reserve(graphNode->inputs.size());
        for (int inputIndex = 0; inputIndex < graphNode->inputs.size(); ++inputIndex) {
            QVariant inputValue;

            for (const NodeConnection &connection : m_graph->connections) {
                if (connection.toNodeId != graphNode->id || connection.toPortIndex != inputIndex) {
                    continue;
                }

                inputValue = valuesByNodeId.value(connection.fromNodeId);
                break;
            }

            if (!inputValue.isValid()) {
                inputValue = defaultValueForPort(graphNode->inputs.at(inputIndex));
            }

            inputs.append(inputValue);
        }

        const QVariant value = evaluateGraphNode(*graphNode, time, inputs);
        valuesByNodeId.insert(nodeId, value);
        cacheValue(cacheKey, value);
        graphNode->dirty = false;
    }

    const QVariant outputValue = valuesByNodeId.value(outputNode->id);
    if (outputValue.canConvert<QImage>()) {
        return outputValue.value<QImage>();
    }

    return transparentImage();
}

void NodeEvaluator::invalidateCache(int nodeId)
{
    if (nodeId < 0) {
        return;
    }

    QSet<int> affectedNodeIds;
    QQueue<int> pendingNodeIds;

    affectedNodeIds.insert(nodeId);
    pendingNodeIds.enqueue(nodeId);

    if (m_graph) {
        while (!pendingNodeIds.isEmpty()) {
            const int currentNodeId = pendingNodeIds.dequeue();
            for (const NodeConnection &connection : m_graph->connections) {
                if (connection.fromNodeId != currentNodeId) {
                    continue;
                }
                if (affectedNodeIds.contains(connection.toNodeId)) {
                    continue;
                }

                affectedNodeIds.insert(connection.toNodeId);
                pendingNodeIds.enqueue(connection.toNodeId);
            }
        }
    }

    QVector<CacheKey> retainedKeys;
    retainedKeys.reserve(m_cacheInsertionOrder.size());

    for (const CacheKey &cacheKey : std::as_const(m_cacheInsertionOrder)) {
        if (affectedNodeIds.contains(cacheKey.nodeId)) {
            m_frameCache.remove(cacheKey);
            continue;
        }
        retainedKeys.append(cacheKey);
    }

    m_cacheInsertionOrder = retainedKeys;
}

void NodeEvaluator::invalidateAll()
{
    m_frameCache.clear();
    m_cacheInsertionOrder.clear();
}

QVariant NodeEvaluator::evaluateGraphNode(const GraphNode &node,
                                          double time,
                                          const QVector<QVariant> &inputs) const
{
    if (m_nodeEvaluatorFn) {
        return m_nodeEvaluatorFn(node, time, inputs);
    }

    if (node.typeName == QLatin1String("Output") && !inputs.isEmpty()) {
        return inputs.first();
    }

    return {};
}

QVariant NodeEvaluator::defaultValueForPort(const NodePort &port) const
{
    switch (port.type) {
    case NodeSocketType::Image:
        return transparentImage();
    case NodeSocketType::Mask:
        // Unconnected masks default to fully opaque white so downstream nodes
        // behave like "no mask" rather than punching holes by default.
        return defaultMaskImage();
    case NodeSocketType::Number:
        return 0.0;
    case NodeSocketType::Color:
        return QColor(Qt::black);
    }

    return {};
}

QImage NodeEvaluator::transparentImage() const
{
    QImage image(m_outputSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

QImage NodeEvaluator::defaultMaskImage() const
{
    QImage image(m_outputSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    return image;
}

qint64 NodeEvaluator::quantizeTimeMs(double time) const
{
    if (!std::isfinite(time)) {
        return 0;
    }

    return qRound64(time * 1000.0);
}

NodeEvaluator::CacheKey NodeEvaluator::makeCacheKey(int nodeId, double time) const
{
    return CacheKey{nodeId, quantizeTimeMs(time)};
}

void NodeEvaluator::cacheValue(const CacheKey &key, const QVariant &value)
{
    if (m_frameCache.contains(key)) {
        m_cacheInsertionOrder.removeOne(key);
    }

    m_frameCache.insert(key, value);
    m_cacheInsertionOrder.append(key);
    evictIfNeeded();
}

void NodeEvaluator::evictIfNeeded()
{
    while (m_cacheInsertionOrder.size() > kMaxCacheEntries) {
        const CacheKey oldestKey = m_cacheInsertionOrder.takeFirst();
        m_frameCache.remove(oldestKey);
    }
}
