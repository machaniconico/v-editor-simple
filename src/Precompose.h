#pragma once

#include "LayerCompositor.h"

#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSize>
#include <QString>
#include <QVector>

// --- Composition ---

struct Composition {
    int     id       = -1;
    QString name;
    int     width    = 1920;
    int     height   = 1080;
    double  fps      = 30.0;
    double  duration = 10.0;         // seconds
    QColor  bgColor  = Qt::transparent;

    QVector<CompositeLayer> layers;

    QJsonObject toJson() const;
    static Composition fromJson(const QJsonObject &obj);
};

// --- Composition tree node (for hierarchy queries) ---

struct CompositionNode {
    int id   = -1;
    QString name;
    QVector<CompositionNode> children;

    QJsonObject toJson() const;
};

// --- Precompose Manager ---

class PrecomposeManager
{
public:
    PrecomposeManager() = default;

    // --- Composition CRUD ---

    int  createComposition(const QString &name, int width, int height,
                           double fps, double duration);
    bool deleteComposition(int id);

    Composition *getComposition(int id);
    const Composition *getComposition(int id) const;

    QVector<Composition> allCompositions() const;

    // --- Precompose / Expand ---

    int  precompose(const QVector<int> &layerIndices,
                    const QString &compositionName,
                    LayerCompositor &parentCompositor);

    bool expandComposition(int compId, LayerCompositor &parentCompositor);

    // --- Rendering ---

    QImage renderComposition(int compId, double time, const QSize &canvasSize) const;

    // --- Main composition ---

    void setMainComposition(int id);
    int  mainCompositionId() const { return m_mainCompId; }

    // --- Hierarchy ---

    CompositionNode compositionTree() const;

    // --- Duplicate ---

    int duplicateComposition(int id, const QString &newName);

    // --- Serialisation ---

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &obj);

private:
    QMap<int, Composition> m_compositions;
    int m_nextId     = 1;
    int m_mainCompId = -1;

    // Recursive helpers
    QImage renderCompositionInternal(int compId, double time,
                                     const QSize &canvasSize,
                                     QVector<int> &visited) const;

    CompositionNode buildTree(int compId, QVector<int> &visited) const;

    // Extract comp-id from a "Composition" layer's sourcePath ("comp:N")
    static int compIdFromLayer(const CompositeLayer &layer);

    // Create a reference layer that points at a composition
    static CompositeLayer makeCompRefLayer(const Composition &comp);
};
