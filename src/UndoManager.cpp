#include "UndoManager.h"

UndoManager::UndoManager(QObject *parent)
    : QObject(parent)
{
}

void UndoManager::saveState(const TimelineState &state, const QString &description)
{
    m_redoStack.clear();
    m_undoStack.push({state, description});
    if (m_undoStack.size() > MAX_UNDO)
        m_undoStack.removeFirst();
    emit stateChanged();
}

TimelineState UndoManager::undo()
{
    if (!canUndo()) return m_undoStack.top().state;
    m_redoStack.push(m_undoStack.pop());
    emit stateChanged();
    return m_undoStack.top().state;
}

TimelineState UndoManager::redo()
{
    if (!canRedo()) return m_undoStack.top().state;
    m_undoStack.push(m_redoStack.pop());
    emit stateChanged();
    return m_undoStack.top().state;
}

QString UndoManager::undoDescription() const
{
    if (!canUndo()) return QString();
    return m_undoStack.top().description;
}

QString UndoManager::redoDescription() const
{
    if (!canRedo()) return QString();
    return m_redoStack.top().description;
}

void UndoManager::clear()
{
    m_undoStack.clear();
    m_redoStack.clear();
    emit stateChanged();
}
