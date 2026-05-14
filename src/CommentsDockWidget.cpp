#include "CommentsDockWidget.h"

#include <QPixmap>
#include <QColor>
#include <QString>
#include <QPoint>
#include <QAction>
#include <QWidget>
#include <QSplitter>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString CommentsDockWidget::formatTimecode(qint64 ms)
{
    if (ms < 0) ms = 0;
    qint64 totalSec = ms / 1000;
    int hh = static_cast<int>(totalSec / 3600);
    int mm = static_cast<int>((totalSec % 3600) / 60);
    int ss = static_cast<int>(totalSec % 60);
    return QString::asprintf("%02d:%02d:%02d", hh, mm, ss);
}

QIcon CommentsDockWidget::avatarIcon(const QColor &color)
{
    QPixmap pix(16, 16);
    pix.fill(color.isValid() ? color : Qt::gray);
    return QIcon(pix);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CommentsDockWidget::CommentsDockWidget(collab::CommentTrack *track, QWidget *parent)
    : QDockWidget(tr("Comments"), parent)
    , m_track(track)
{
    setObjectName(QStringLiteral("CommentsDockWidget"));

    // --- root container ---
    QWidget *root = new QWidget(this);
    QVBoxLayout *rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    // --- toolbar row ---
    QHBoxLayout *toolRow = new QHBoxLayout;
    toolRow->setSpacing(6);

    m_timecodeLabel = new QLabel(QStringLiteral("00:00:00"), root);
    m_timecodeLabel->setToolTip(tr("Current timecode"));

    m_addBtn = new QPushButton(tr("Add Comment at current time"), root);
    m_addBtn->setToolTip(tr("Add a new top-level comment at the current playhead position"));

    toolRow->addWidget(m_timecodeLabel);
    toolRow->addWidget(m_addBtn, 1);
    rootLayout->addLayout(toolRow);

    // --- tree ---
    m_tree = new QTreeWidget(root);
    m_tree->setColumnCount(1);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setExpandsOnDoubleClick(false);
    rootLayout->addWidget(m_tree, 1);

    // --- reply area ---
    m_replyEdit = new QPlainTextEdit(root);
    m_replyEdit->setPlaceholderText(tr("Write a reply…"));
    m_replyEdit->setMaximumHeight(70);

    m_replyBtn = new QPushButton(tr("Reply"), root);
    m_replyBtn->setEnabled(false);

    QHBoxLayout *replyRow = new QHBoxLayout;
    replyRow->addWidget(m_replyEdit, 1);
    replyRow->addWidget(m_replyBtn);
    rootLayout->addLayout(replyRow);

    root->setLayout(rootLayout);
    setWidget(root);

    // --- connections ---
    connect(m_addBtn, &QPushButton::clicked, this, &CommentsDockWidget::onAddComment);
    connect(m_replyBtn, &QPushButton::clicked, this, &CommentsDockWidget::onReply);
    connect(m_tree, &QTreeWidget::itemClicked,
            this, &CommentsDockWidget::onItemClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &CommentsDockWidget::onCustomContextMenu);
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
                m_replyBtn->setEnabled(current != nullptr);
            });

    refreshTree();
}

// ---------------------------------------------------------------------------
// Tree building
// ---------------------------------------------------------------------------

QTreeWidgetItem *CommentsDockWidget::buildItem(const collab::Comment &c)
{
    QTreeWidgetItem *item = new QTreeWidgetItem;
    item->setData(0, Qt::UserRole, c.id);                    // store id
    item->setData(0, Qt::UserRole + 1, c.timecodeMs);        // store timecode

    // avatar color: derive from authorId hash for demo (no User lookup here)
    QColor avatarColor;
    if (!c.authorId.isEmpty()) {
        uint hash = qHash(c.authorId);
        avatarColor = QColor::fromHsv(static_cast<int>(hash % 360), 180, 200);
    } else {
        avatarColor = Qt::gray;
    }
    item->setIcon(0, avatarIcon(avatarColor));

    // label: "authorId  HH:MM:SS  body (preview)"
    QString preview = c.body.length() > 60 ? c.body.left(60) + QStringLiteral("…") : c.body;
    QString statusBadge;
    if (c.status == collab::Status::Resolved) {
        statusBadge = QStringLiteral(" [Resolved]");
    } else if (c.status == collab::Status::Deleted) {
        statusBadge = QStringLiteral(" [Deleted]");
    }
    QString label = QStringLiteral("%1  %2  %3%4")
                        .arg(c.authorId)
                        .arg(formatTimecode(c.timecodeMs))
                        .arg(preview)
                        .arg(statusBadge);
    item->setText(0, label);

    if (c.status == collab::Status::Resolved) {
        item->setForeground(0, QColor(Qt::gray));
    } else if (c.status == collab::Status::Deleted) {
        item->setForeground(0, QColor(Qt::lightGray));
    }

    return item;
}

void CommentsDockWidget::refreshTree()
{
    m_tree->clear();

    if (!m_track) return;

    const QList<collab::Comment> topLevel = m_track->topLevelComments();
    for (const collab::Comment &c : topLevel) {
        QTreeWidgetItem *topItem = buildItem(c);
        m_tree->addTopLevelItem(topItem);

        const QList<collab::Comment> replies = m_track->repliesOf(c.id);
        for (const collab::Comment &r : replies) {
            QTreeWidgetItem *replyItem = buildItem(r);
            topItem->addChild(replyItem);
        }
        topItem->setExpanded(true);
    }
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CommentsDockWidget::onAddComment()
{
    // Emit with 0 as stub; caller connects and provides actual playhead timecode
    emit addCommentRequested(0);
}

void CommentsDockWidget::onReply()
{
    if (!m_track) return;

    QTreeWidgetItem *current = m_tree->currentItem();
    if (!current) return;

    QString text = m_replyEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;

    // Walk up to find top-level comment id (the parentId to reply to)
    QTreeWidgetItem *target = current;
    while (target->parent() != nullptr) {
        target = target->parent();
    }
    QString parentId = target->data(0, Qt::UserRole).toString();

    m_track->replyTo(parentId, QStringLiteral("me"), text);
    m_replyEdit->clear();
    refreshTree();
    emit commentChanged();
}

void CommentsDockWidget::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    qint64 tc = item->data(0, Qt::UserRole + 1).toLongLong();
    emit commentSelected(tc);
}

void CommentsDockWidget::onCustomContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item || !m_track) return;

    QString commentId = item->data(0, Qt::UserRole).toString();

    QMenu menu(this);
    QAction *resolveAct = menu.addAction(tr("Mark Resolved"));
    QAction *deleteAct  = menu.addAction(tr("Delete"));

    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == resolveAct) {
        m_track->markResolved(commentId);
        refreshTree();
        emit commentChanged();
    } else if (chosen == deleteAct) {
        // Mark as Deleted by finding and modifying the comment
        for (collab::Comment &c : m_track->comments) {
            if (c.id == commentId) {
                c.status = collab::Status::Deleted;
                m_track->version++;
                break;
            }
        }
        refreshTree();
        emit commentChanged();
    }
}
