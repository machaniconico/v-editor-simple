#pragma once

#include "NodeGraph.h"

#include <QHash>
#include <QImage>
#include <QSize>
#include <QtGlobal>
#include <QVariant>
#include <QVector>

#include <cstddef>
#include <functional>

class NodeEvaluator
{
public:
    using NodeEvaluatorFn = std::function<QVariant(const GraphNode &node,
                                                   double time,
                                                   const QVector<QVariant> &inputs)>;

    void setGraph(NodeGraph *graph);
    void setOutputSize(QSize size);
    void setNodeEvaluatorFn(NodeEvaluatorFn evaluatorFn);

    QImage render(double time);

    // Removes cached values for the specified node and all downstream nodes.
    void invalidateCache(int nodeId);
    void invalidateAll();

    // Exposed so the Qt6 qHash overload can be declared out-of-line without
    // relying on private nested-type access rules.
    struct CacheKey {
        int nodeId = -1;
        qint64 quantizedTimeMs = 0;

        bool operator==(const CacheKey &other) const noexcept
        {
            return nodeId == other.nodeId
                && quantizedTimeMs == other.quantizedTimeMs;
        }

        friend size_t qHash(const CacheKey &key, size_t seed) noexcept;
    };

private:
    static constexpr int kMaxCacheEntries = 500;

    QVariant evaluateGraphNode(const GraphNode &node,
                               double time,
                               const QVector<QVariant> &inputs) const;
    QVariant defaultValueForPort(const NodePort &port) const;
    QImage transparentImage() const;
    QImage defaultMaskImage() const;
    qint64 quantizeTimeMs(double time) const;
    CacheKey makeCacheKey(int nodeId, double time) const;
    void cacheValue(const CacheKey &key, const QVariant &value);
    void evictIfNeeded();

    NodeGraph *m_graph = nullptr;
    QSize m_outputSize;
    NodeEvaluatorFn m_nodeEvaluatorFn;
    QHash<CacheKey, QVariant> m_frameCache;
    QVector<CacheKey> m_cacheInsertionOrder;
};

inline size_t qHash(const NodeEvaluator::CacheKey &key, size_t seed = 0) noexcept
{
    seed ^= ::qHash(key.nodeId, seed + 0x9e3779b9U);
    seed ^= ::qHash(key.quantizedTimeMs, seed + 0x85ebca6bU);
    return seed;
}
