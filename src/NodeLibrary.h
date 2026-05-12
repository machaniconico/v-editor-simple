#pragma once

#include "NodeGraph.h"

#include <QColor>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

namespace nodelib {

struct NodeTypeDescriptor {
    QString typeName;
    QString displayName;
    QString category;
    QVector<NodePort> inputs;
    QVector<NodePort> outputs;
    QVariantMap defaultParams;
};

class NodeRegistry
{
public:
    static NodeRegistry& instance();

    void registerType(const NodeTypeDescriptor& descriptor);
    const NodeTypeDescriptor* descriptor(const QString& typeName) const;
    QStringList allTypeNames() const;

private:
    NodeRegistry() = default;
    QHash<QString, NodeTypeDescriptor> m_types;
};

QVariant evaluateBuiltinNode(const GraphNode& node,
                             double time,
                             const QVector<QVariant>& inputs,
                             QSize outputSize);

void registerBuiltinNodes();

} // namespace nodelib
