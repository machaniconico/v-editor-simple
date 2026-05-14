#include "CollabHistoryDialog.h"
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDateTime>

CollabHistoryDialog::CollabHistoryDialog(collab::history::HistoryLog *log, QWidget *parent)
    : QDialog(parent)
    , m_log(log)
{
    setWindowTitle(QStringLiteral("Change History"));
    setWindowFlags(Qt::Window);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("Timestamp"),
         QStringLiteral("Author"),
         QStringLiteral("Action"),
         QStringLiteral("Description")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_revertBtn = new QPushButton(QStringLiteral("Revert to selected"), this);
    m_revertBtn->setEnabled(false);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_revertBtn->setEnabled(!m_table->selectedItems().isEmpty());
    });
    connect(m_revertBtn, &QPushButton::clicked, this, &CollabHistoryDialog::onRevertClicked);

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    btnLayout->addWidget(m_revertBtn);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_table);
    mainLayout->addLayout(btnLayout);

    populateTable();
    resize(720, 400);
}

void CollabHistoryDialog::populateTable()
{
    if (!m_log)
        return;

    const QList<collab::history::HistoryEntry> entries = m_log->entries();
    m_table->setRowCount(entries.size());

    for (int i = 0; i < entries.size(); ++i) {
        const collab::history::HistoryEntry &e = entries.at(i);

        const QString ts =
            QDateTime::fromMSecsSinceEpoch(e.timestampMs).toString(Qt::ISODate);

        m_table->setItem(i, 0, new QTableWidgetItem(ts));
        m_table->setItem(i, 1, new QTableWidgetItem(e.authorId));
        m_table->setItem(i, 2, new QTableWidgetItem(e.action));
        m_table->setItem(i, 3, new QTableWidgetItem(e.description));

        // Store snapshotHash in the first column's UserRole for retrieval
        m_table->item(i, 0)->setData(Qt::UserRole, e.snapshotHash);
    }
}

void CollabHistoryDialog::onRevertClicked()
{
    const QList<QTableWidgetItem *> selected = m_table->selectedItems();
    if (selected.isEmpty())
        return;

    const int row = selected.first()->row();
    QTableWidgetItem *tsItem = m_table->item(row, 0);
    if (!tsItem)
        return;

    const QString snapshotHash = tsItem->data(Qt::UserRole).toString();
    emit revertRequested(snapshotHash);
}
