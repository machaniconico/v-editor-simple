#pragma once

#include <QObject>
#include <QVector>
#include <QStack>
#include <QStringList>
#include <functional>
#include "Timeline.h"

struct TimelineState {
    // One ClipInfo vector per VIDEO row (videoTracks[0] = V1, [1] = V2, ...)
    // and per AUDIO row (audioTracks[0] = A1, [1] = A2, ...). Earlier
    // single-track versions only stored V1+A1 here, so undo silently
    // dropped any clip that lived on V2/V3 — this multi-track layout
    // restores the whole timeline state.
    QVector<QVector<ClipInfo>> videoTracks;
    QVector<QVector<ClipInfo>> audioTracks;
    // Selection is V1-relative for now (matches the rest of the editor's
    // current selection semantics). Track-aware selection can extend this
    // later without breaking the persistence shape.
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

    int currentIndex() const { return m_undoStack.size() - 1; }

    QStringList historyDescriptions() const;

    bool jumpTo(int index);

    void clear();

signals:
    void stateChanged();
    void historyChanged();
    void stateJumpRequested(const TimelineState &state);

private:
    struct Entry {
        TimelineState state;
        QString description;
    };

    QStack<Entry> m_undoStack;
    QStack<Entry> m_redoStack;
    static constexpr int MAX_UNDO = 100;
};
