#pragma once

#include <QDockWidget>
#include <QListWidget>

class UndoManager;

class HistoryDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit HistoryDockWidget(UndoManager *undoManager, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onItemClicked(QListWidgetItem *item);

private:
    UndoManager *m_undoManager;
    QListWidget *m_listWidget;
};
