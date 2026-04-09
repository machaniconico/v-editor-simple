#pragma once

#include <QObject>
#include <QVector>
#include <QStack>
#include <functional>
#include "Timeline.h"

struct TimelineState {
    QVector<ClipInfo> videoClips;
    QVector<ClipInfo> audioClips;
    int selectedClip = -1;
    double playheadPos = 0.0;
};

class UndoManager : public QObject
{
    Q_OBJECT

public:
    explicit UndoManager(QObject *parent = nullptr);

    void saveState(const TimelineState &state, const QString &description);
    bool canUndo() const { return m_undoStack.size() > 1; }
    bool canRedo() const { return !m_redoStack.isEmpty(); }

    TimelineState undo();
    TimelineState redo();

    QString undoDescription() const;
    QString redoDescription() const;

    void clear();

signals:
    void stateChanged();

private:
    struct Entry {
        TimelineState state;
        QString description;
    };

    QStack<Entry> m_undoStack;
    QStack<Entry> m_redoStack;
    static constexpr int MAX_UNDO = 100;
};
