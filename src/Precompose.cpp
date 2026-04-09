#include "Precompose.h"

#include <algorithm>

// ===== Composition — serialisation =====

QJsonObject Composition::toJson() const
{
    QJsonObject obj;
    obj["id"]       = id;
    obj["name"]     = name;
    obj["width"]    = width;
    obj["height"]   = height;
    obj["fps"]      = fps;
    obj["duration"] = duration;

    obj["bgColorR"] = bgColor.red();
    obj["bgColorG"] = bgColor.green();
    obj["bgColorB"] = bgColor.blue();
    obj["bgColorA"] = bgColor.alpha();

    QJsonArray arr;
    for (const auto &l : layers)
        arr.append(l.toJson());
    obj["layers"] = arr;

    return obj;
}

Composition Composition::fromJson(const QJsonObject &obj)
{
    Composition c;
    c.id       = obj["id"].toInt(-1);
    c.name     = obj["name"].toString();
    c.width    = obj["width"].toInt(1920);
    c.height   = obj["height"].toInt(1080);
    c.fps      = obj["fps"].toDouble(30.0);
    c.duration = obj["duration"].toDouble(10.0);

    c.bgColor = QColor(
        obj["bgColorR"].toInt(0),
        obj["bgColorG"].toInt(0),
        obj["bgColorB"].toInt(0),
        obj["bgColorA"].toInt(0));

    const QJsonArray arr = obj["layers"].toArray();
    for (const auto &v : arr)
        c.layers.append(CompositeLayer::fromJson(v.toObject()));

    return c;
}

// ===== CompositionNode — serialisation =====

QJsonObject CompositionNode::toJson() const
{
    QJsonObject obj;
    obj["id"]   = id;
    obj["name"] = name;

    QJsonArray arr;
    for (const auto &child : children)
        arr.append(child.toJson());
    obj["children"] = arr;

    return obj;
}

// ===== PrecomposeManager — Composition CRUD =====

int PrecomposeManager::createComposition(const QString &name, int width, int height,
                                         double fps, double duration)
{
    Composition comp;
    comp.id       = m_nextId++;
    comp.name     = name;
    comp.width    = width;
    comp.height   = height;
    comp.fps      = fps;
    comp.duration = duration;

    m_compositions.insert(comp.id, comp);

    // First composition becomes the main by default
    if (m_mainCompId < 0)
        m_mainCompId = comp.id;

    return comp.id;
}

bool PrecomposeManager::deleteComposition(int id)
{
    if (!m_compositions.contains(id))
        return false;

    m_compositions.remove(id);

    if (m_mainCompId == id)
        m_mainCompId = m_compositions.isEmpty() ? -1 : m_compositions.firstKey();

    return true;
}

Composition *PrecomposeManager::getComposition(int id)
{
    auto it = m_compositions.find(id);
    return (it != m_compositions.end()) ? &it.value() : nullptr;
}

const Composition *PrecomposeManager::getComposition(int id) const
{
    auto it = m_compositions.find(id);
    return (it != m_compositions.end()) ? &it.value() : nullptr;
}

QVector<Composition> PrecomposeManager::allCompositions() const
{
    QVector<Composition> result;
    result.reserve(m_compositions.size());
    for (auto it = m_compositions.constBegin(); it != m_compositions.constEnd(); ++it)
        result.append(it.value());
    return result;
}

// ===== Precompose / Expand =====

int PrecomposeManager::precompose(const QVector<int> &layerIndices,
                                  const QString &compositionName,
                                  LayerCompositor &parentCompositor)
{
    if (layerIndices.isEmpty())
        return -1;

    const QVector<CompositeLayer> parentLayers = parentCompositor.layers();

    // Validate indices
    for (int idx : layerIndices) {
        if (idx < 0 || idx >= parentLayers.size())
            return -1;
    }

    // Determine time range and canvas from selected layers
    double minIn  = std::numeric_limits<double>::max();
    double maxOut = 0.0;
    for (int idx : layerIndices) {
        const auto &l = parentLayers[idx];
        minIn  = qMin(minIn, l.inPoint);
        maxOut = qMax(maxOut, l.outPoint);
    }
    if (maxOut <= 0.0)
        maxOut = 10.0;  // fallback

    // Use main composition dimensions or defaults
    int w = 1920, h = 1080;
    double fps = 30.0;
    if (m_mainCompId >= 0) {
        if (const Composition *main = getComposition(m_mainCompId)) {
            w   = main->width;
            h   = main->height;
            fps = main->fps;
        }
    }

    // Create the new composition
    int compId = createComposition(compositionName, w, h, fps, maxOut - minIn);
    Composition *comp = getComposition(compId);

    // Collect selected layers into the new composition, adjusting time offsets
    QVector<CompositeLayer> selectedLayers;
    for (int idx : layerIndices) {
        CompositeLayer l = parentLayers[idx];
        l.inPoint  -= minIn;
        l.outPoint -= minIn;
        selectedLayers.append(l);
    }
    comp->layers = selectedLayers;

    // Remove selected layers from parent (in reverse order to preserve indices)
    QVector<int> sorted = layerIndices;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int idx : sorted)
        parentCompositor.removeLayer(idx);

    // Insert a single reference layer into the parent
    CompositeLayer ref = makeCompRefLayer(*comp);
    ref.inPoint  = minIn;
    ref.outPoint = maxOut;
    parentCompositor.addLayer(ref);

    return compId;
}

bool PrecomposeManager::expandComposition(int compId, LayerCompositor &parentCompositor)
{
    const Composition *comp = getComposition(compId);
    if (!comp)
        return false;

    // Find the reference layer in the parent
    const QVector<CompositeLayer> parentLayers = parentCompositor.layers();
    int refIndex = -1;
    for (int i = 0; i < parentLayers.size(); ++i) {
        if (compIdFromLayer(parentLayers[i]) == compId) {
            refIndex = i;
            break;
        }
    }

    if (refIndex < 0)
        return false;

    double baseTime = parentLayers[refIndex].inPoint;

    // Remove the reference layer
    parentCompositor.removeLayer(refIndex);

    // Insert the composition's layers back, adjusting time offsets
    for (const auto &l : comp->layers) {
        CompositeLayer restored = l;
        restored.inPoint  += baseTime;
        restored.outPoint += baseTime;
        parentCompositor.addLayer(restored);
    }

    // Remove the composition itself
    deleteComposition(compId);

    return true;
}

// ===== Rendering =====

QImage PrecomposeManager::renderComposition(int compId, double time,
                                            const QSize &canvasSize) const
{
    QVector<int> visited;
    return renderCompositionInternal(compId, time, canvasSize, visited);
}

QImage PrecomposeManager::renderCompositionInternal(int compId, double time,
                                                    const QSize &canvasSize,
                                                    QVector<int> &visited) const
{
    // Guard against circular references
    if (visited.contains(compId))
        return QImage(canvasSize, QImage::Format_ARGB32);

    const Composition *comp = getComposition(compId);
    if (!comp)
        return QImage(canvasSize, QImage::Format_ARGB32);

    visited.append(compId);

    // Build the layer list, recursively rendering sub-compositions
    QVector<CompositeLayer> resolvedLayers;
    for (const auto &layer : comp->layers) {
        int subCompId = compIdFromLayer(layer);
        if (subCompId >= 0) {
            // Recursively render the sub-composition
            double localTime = time - layer.inPoint;
            if (localTime < 0.0)
                continue;

            QSize subSize(comp->width, comp->height);
            QImage subImage = renderCompositionInternal(subCompId, localTime,
                                                        subSize, visited);

            // Create a temporary solid-like layer carrying the rendered image
            // We write it to a temp path — in a real pipeline this would use
            // an in-memory frame cache. For now we treat it as an image layer.
            CompositeLayer imgLayer = layer;
            imgLayer.sourceType = LayerSourceType::Solid;
            imgLayer.solidColor = Qt::transparent;
            // The actual blitting is handled below by compositing subImage directly.
            // We skip this layer and blend manually.

            // Blend sub-composition image onto a transparent canvas at layer position
            // using the LayerCompositor blend utilities.
            Q_UNUSED(imgLayer);
            Q_UNUSED(subImage);

            // For proper sub-composition rendering we composite via blendImages
            // after the main pass — append the original layer so compositeFrame
            // skips it (sourceType check), then overlay the rendered sub-image.
            resolvedLayers.append(layer);
        } else {
            resolvedLayers.append(layer);
        }
    }

    // First pass: composite non-composition layers via LayerCompositor
    QImage result = LayerCompositor::compositeFrame(resolvedLayers, canvasSize, time);

    // Second pass: overlay sub-composition renders
    for (const auto &layer : comp->layers) {
        int subCompId = compIdFromLayer(layer);
        if (subCompId < 0)
            continue;

        if (!layer.visible)
            continue;
        if (time < layer.inPoint)
            continue;
        if (layer.outPoint > 0.0 && time > layer.outPoint)
            continue;

        double localTime = time - layer.inPoint;
        QSize subSize(comp->width, comp->height);
        QImage subImage = renderCompositionInternal(subCompId, localTime,
                                                    subSize, visited);

        if (!subImage.isNull())
            result = LayerCompositor::blendImages(result, subImage,
                                                  layer.blendMode, layer.opacity);
    }

    // Fill background if not transparent
    if (comp->bgColor.alpha() > 0) {
        QImage bg(canvasSize, QImage::Format_ARGB32);
        bg.fill(comp->bgColor);
        result = LayerCompositor::blendImages(bg, result, BlendMode::Normal, 1.0);
    }

    visited.removeOne(compId);
    return result;
}

// ===== Main composition =====

void PrecomposeManager::setMainComposition(int id)
{
    if (m_compositions.contains(id))
        m_mainCompId = id;
}

// ===== Hierarchy =====

CompositionNode PrecomposeManager::compositionTree() const
{
    if (m_mainCompId < 0)
        return CompositionNode{};

    QVector<int> visited;
    return buildTree(m_mainCompId, visited);
}

CompositionNode PrecomposeManager::buildTree(int compId, QVector<int> &visited) const
{
    CompositionNode node;
    node.id = compId;

    const Composition *comp = getComposition(compId);
    if (!comp)
        return node;

    node.name = comp->name;

    if (visited.contains(compId))
        return node;  // avoid cycles

    visited.append(compId);

    for (const auto &layer : comp->layers) {
        int subId = compIdFromLayer(layer);
        if (subId >= 0)
            node.children.append(buildTree(subId, visited));
    }

    visited.removeOne(compId);
    return node;
}

// ===== Duplicate =====

int PrecomposeManager::duplicateComposition(int id, const QString &newName)
{
    const Composition *src = getComposition(id);
    if (!src)
        return -1;

    int newId = createComposition(
        newName.isEmpty() ? src->name + " (Copy)" : newName,
        src->width, src->height, src->fps, src->duration);

    Composition *dst = getComposition(newId);
    dst->bgColor = src->bgColor;
    dst->layers  = src->layers;   // deep copy (QVector of value types)

    return newId;
}

// ===== Serialisation =====

QJsonObject PrecomposeManager::toJson() const
{
    QJsonObject obj;
    obj["mainCompId"] = m_mainCompId;
    obj["nextId"]     = m_nextId;

    QJsonArray arr;
    for (auto it = m_compositions.constBegin(); it != m_compositions.constEnd(); ++it)
        arr.append(it.value().toJson());
    obj["compositions"] = arr;

    return obj;
}

void PrecomposeManager::fromJson(const QJsonObject &obj)
{
    m_compositions.clear();
    m_mainCompId = obj["mainCompId"].toInt(-1);
    m_nextId     = obj["nextId"].toInt(1);

    const QJsonArray arr = obj["compositions"].toArray();
    for (const auto &v : arr) {
        Composition c = Composition::fromJson(v.toObject());
        m_compositions.insert(c.id, c);
    }
}

// ===== Internal helpers =====

int PrecomposeManager::compIdFromLayer(const CompositeLayer &layer)
{
    // Convention: a composition reference layer uses sourcePath = "comp:<id>"
    if (layer.sourcePath.startsWith("comp:")) {
        bool ok = false;
        int id = layer.sourcePath.mid(5).toInt(&ok);
        return ok ? id : -1;
    }
    return -1;
}

CompositeLayer PrecomposeManager::makeCompRefLayer(const Composition &comp)
{
    CompositeLayer ref;
    ref.name       = comp.name;
    ref.sourceType = LayerSourceType::Video;   // treated as video in the timeline
    ref.sourcePath = QString("comp:%1").arg(comp.id);
    ref.visible    = true;
    ref.locked     = false;
    ref.opacity    = 1.0;
    ref.blendMode  = BlendMode::Normal;
    ref.inPoint    = 0.0;
    ref.outPoint   = comp.duration;
    return ref;
}
