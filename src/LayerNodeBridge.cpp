#include "LayerNodeBridge.h"

namespace layerbridge {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static QString effectTypeToNodeType(VideoEffectType type)
{
    switch (type) {
    case VideoEffectType::Blur:      return QStringLiteral("GaussianBlur");
    case VideoEffectType::Invert:    return QStringLiteral("Invert");
    // Unmapped — fall back to a generic Transform passthrough.
    // These cannot be round-tripped by toEffectStack.
    case VideoEffectType::Sharpen:
    case VideoEffectType::Mosaic:
    case VideoEffectType::ChromaKey:
    case VideoEffectType::Vignette:
    case VideoEffectType::Sepia:
    case VideoEffectType::Grayscale:
    case VideoEffectType::Noise:
    case VideoEffectType::None:
    default:
        return QStringLiteral("Transform");
    }
}

static bool nodeTypeToEffect(const QString &typeName,
                             const QVariantMap &params,
                             VideoEffect &outEffect)
{
    if (typeName == QLatin1String("GaussianBlur")) {
        outEffect = VideoEffect::createBlur(params.value(QStringLiteral("radius"), 0.0).toDouble());
        return true;
    }
    if (typeName == QLatin1String("Invert")) {
        outEffect = VideoEffect::createInvert();
        return true;
    }
    return false; // unmapped
}

// ---------------------------------------------------------------------------
// isLinearChain
// ---------------------------------------------------------------------------

bool isLinearChain(const NodeGraph &graph)
{
    // Must have at least 2 nodes (ImageInput + Output).
    if (graph.nodes.size() < 2)
        return false;

    int inputNodeId  = -1;
    int outputNodeId = -1;

    for (const auto &n : graph.nodes) {
        if (n.typeName == QLatin1String("ImageInput")) {
            if (inputNodeId != -1) return false; // duplicate
            inputNodeId = n.id;
        } else if (n.typeName == QLatin1String("Output")) {
            if (outputNodeId != -1) return false; // duplicate
            outputNodeId = n.id;
        }
    }
    if (inputNodeId == -1 || outputNodeId == -1)
        return false;

    // Count incoming / outgoing connections per node.
    QHash<int, int> incoming;
    QHash<int, int> outgoing;
    for (const auto &c : graph.connections) {
        outgoing[c.fromNodeId]++;
        incoming[c.toNodeId]++;
    }

    for (const auto &n : graph.nodes) {
        int inCount  = incoming.value(n.id, 0);
        int outCount = outgoing.value(n.id, 0);

        if (n.id == inputNodeId) {
            if (outCount != 1) return false;
            // ImageInput has no inputs by design; ignore any incoming.
        } else if (n.id == outputNodeId) {
            if (inCount != 1) return false;
            // Output has no outputs; ignore any outgoing.
        } else {
            if (inCount != 1 || outCount != 1)
                return false;
        }
    }

    // Walk the chain from ImageInput to Output to verify connectivity.
    int currentId = inputNodeId;
    int visited = 0;
    while (currentId != outputNodeId) {
        bool foundNext = false;
        for (const auto &c : graph.connections) {
            if (c.fromNodeId == currentId) {
                currentId = c.toNodeId;
                foundNext = true;
                break;
            }
        }
        if (!foundNext)
            return false; // chain broken before reaching Output
        visited++;
        if (visited > graph.nodes.size())
            return false; // cycle guard
    }

    // Every node must be on the chain.
    return visited == graph.nodes.size() - 1;
}

// ---------------------------------------------------------------------------
// fromEffectStack
// ---------------------------------------------------------------------------

NodeGraph fromEffectStack(const QString &clipId, const QVector<VideoEffect> &effects)
{
    NodeGraph graph;

    // 1. ImageInput node
    int inputId = graph.addNode(QStringLiteral("ImageInput"));
    GraphNode *inputNode = graph.node(inputId);
    if (inputNode) {
        inputNode->params.insert(QStringLiteral("clipId"), clipId);
        inputNode->canvasPos = QPointF(50, 50);
    }

    // 2. One node per effect, in order
    int prevId = inputId;
    for (int i = 0; i < effects.size(); ++i) {
        const VideoEffect &eff = effects[i];
        QString nodeType = effectTypeToNodeType(eff.type);

        int nid = graph.addNode(nodeType);
        GraphNode *node = graph.node(nid);
        if (node) {
            node->canvasPos = QPointF(50 + (i + 1) * 200, 50);

            // Map effect params → node params for mappable types.
            if (nodeType == QLatin1String("GaussianBlur")) {
                node->params.insert(QStringLiteral("radius"), eff.param1);
            }
            // Invert has no params; unmapped Transform nodes keep defaults.
        }

        graph.connect(prevId, 0, nid, 0);
        prevId = nid;
    }

    // 3. Output node
    int outputId = graph.addNode(QStringLiteral("Output"));
    GraphNode *outputNode = graph.node(outputId);
    if (outputNode) {
        int stepCount = effects.size() + 1;
        outputNode->canvasPos = QPointF(50 + stepCount * 200, 50);
    }
    graph.connect(prevId, 0, outputId, 0);

    return graph;
}

// ---------------------------------------------------------------------------
// toEffectStack
// ---------------------------------------------------------------------------

bool toEffectStack(const NodeGraph &graph, QVector<VideoEffect> &outEffects)
{
    if (!isLinearChain(graph))
        return false;

    // Walk the chain from ImageInput to Output.
    int currentId = -1;
    for (const auto &n : graph.nodes) {
        if (n.typeName == QLatin1String("ImageInput")) {
            currentId = n.id;
            break;
        }
    }
    if (currentId == -1)
        return false;

    QVector<VideoEffect> result;

    while (true) {
        // Find the outgoing connection from current node.
        int nextId = -1;
        for (const auto &c : graph.connections) {
            if (c.fromNodeId == currentId) {
                nextId = c.toNodeId;
                break;
            }
        }
        if (nextId == -1)
            break; // end of chain

        const GraphNode *nextNode = graph.node(nextId);
        if (!nextNode)
            return false;

        // Reached Output — stop.
        if (nextNode->typeName == QLatin1String("Output"))
            break;

        // Try to map node type back to VideoEffect.
        VideoEffect eff;
        if (!nodeTypeToEffect(nextNode->typeName, nextNode->params, eff))
            return false; // unmapped node type — cannot round-trip

        result.append(eff);
        currentId = nextId;
    }

    outEffects = result;
    return true;
}

} // namespace layerbridge
