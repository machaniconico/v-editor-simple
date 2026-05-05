#include "HistoryDockWidget.h"
#include "UndoManager.h"
#include <QVBoxLayout>
#include <QFont>
#include <QColor>

HistoryDockWidget::HistoryDockWidget(UndoManager *undoManager, QWidget *parent)
    : QDockWidget("History", parent)
    , m_undoManager(undoManager)
{
    setObjectName("HistoryDockWidget");

    m_listWidget = new QListWidget(this);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    setWidget(m_listWidget);

    connect(m_listWidget, &QListWidget::itemClicked,
            this, &HistoryDockWidget::onItemClicked);

    if (m_undoManager) {
        connect(m_undoManager, &UndoManager::historyChanged,
                this, &HistoryDockWidget::refresh);
        refresh();
    }
}

void HistoryDockWidget::refresh()
{
    if (!m_undoManager)
        return;

    const QStringList descriptions = m_undoManager->historyDescriptions();
    const int current = m_undoManager->currentIndex();

    m_listWidget->blockSignals(true);
    m_listWidget->clear();

    QFont normalFont = m_listWidget->font();
    QFont currentFont = normalFont;
    currentFont.setBold(true);

    for (int i = 0; i < descriptions.size(); ++i) {
        m_listWidget->addItem(descriptions[i]);
        QListWidgetItem *item = m_listWidget->item(i);
        if (i == current) {
            item->setFont(currentFont);
            item->setForeground(Qt::white);
            item->setBackground(QColor(70, 130, 180));
        } else if (i < current) {
            item->setForeground(QColor(200, 200, 200));
        } else {
            item->setForeground(QColor(140, 140, 140));
        }
    }

    m_listWidget->scrollToItem(m_listWidget->item(current),
                               QAbstractItemView::EnsureVisible);

    m_listWidget->blockSignals(false);
}

void HistoryDockWidget::onItemClicked(QListWidgetItem *item)
{
    if (!m_undoManager)
        return;

    const int row = m_listWidget->row(item);
    m_undoManager->jumpTo(row);
}
