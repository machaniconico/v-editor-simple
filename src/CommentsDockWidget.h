#pragma once

#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QMenu>

#include "CollaborationModel.h"

class CommentsDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit CommentsDockWidget(collab::CommentTrack *track, QWidget *parent = nullptr);

signals:
    void addCommentRequested(qint64 timecodeMs);
    void commentSelected(qint64 timecodeMs);
    void commentChanged();

private slots:
    void onAddComment();
    void onReply();
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onCustomContextMenu(const QPoint &pos);

private:
    void refreshTree();
    QTreeWidgetItem *buildItem(const collab::Comment &c);
    static QString formatTimecode(qint64 ms);
    static QIcon avatarIcon(const QColor &color);

    collab::CommentTrack *m_track;

    QLabel          *m_timecodeLabel;
    QPushButton     *m_addBtn;
    QTreeWidget     *m_tree;
    QPlainTextEdit  *m_replyEdit;
    QPushButton     *m_replyBtn;
};
