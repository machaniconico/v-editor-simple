#pragma once

#include "CollabHistoryLog.h"
#include <QDialog>
#include <QString>

class QTableWidget;
class QPushButton;

class CollabHistoryDialog : public QDialog {
    Q_OBJECT

public:
    explicit CollabHistoryDialog(collab::history::HistoryLog *log, QWidget *parent = nullptr);

signals:
    void revertRequested(const QString &snapshotHash);

private slots:
    void onRevertClicked();

private:
    collab::history::HistoryLog *m_log;
    QTableWidget               *m_table;
    QPushButton                *m_revertBtn;

    void populateTable();
};
