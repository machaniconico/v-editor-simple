#pragma once

#include "NodeGraph.h"

#include <QScrollArea>
#include <QVariant>
#include <QWidget>

class QLabel;
class QVBoxLayout;

class NodePropertiesPanel : public QScrollArea
{
    Q_OBJECT
public:
    explicit NodePropertiesPanel(QWidget *parent = nullptr);

    void setSelection(NodeGraph *graph, int nodeId);

signals:
    void paramChanged(int nodeId, const QString &key, const QVariant &value);

private:
    void buildForm(NodeGraph *graph, int nodeId);
    void buildPlaceholder();
    void clearForm();

    NodeGraph *m_graph = nullptr;
    int m_nodeId = -1;
    QWidget *m_formWidget = nullptr;
    QVBoxLayout *m_layout = nullptr;
};
