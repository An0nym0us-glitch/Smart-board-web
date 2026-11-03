#include "document/Command.h"

#include <algorithm>

namespace cb {

CompositeCommand::CompositeCommand(QString text)
    : m_text(std::move(text))
{
}

void CompositeCommand::add(CommandPtr cmd)
{
    if (cmd)
        m_children.push_back(std::move(cmd));
}

void CompositeCommand::redo(Document& doc)
{
    for (auto& c : m_children)
        c->redo(doc);
}

void CompositeCommand::undo(Document& doc)
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
        (*it)->undo(doc);
}

PageId CompositeCommand::pageId() const
{
    for (const auto& c : m_children)
        if (!c->pageId().isNull())
            return c->pageId();
    return PageId();
}

CommandStack::CommandStack(Document& doc, QObject* parent)
    : QObject(parent)
    , m_doc(doc)
{
}

CommandStack::~CommandStack() = default;

void CommandStack::push(CommandPtr cmd)
{
    if (!cmd)
        return;
    cmd->redo(m_doc);
    record(std::move(cmd));
}

void CommandStack::pushApplied(CommandPtr cmd)
{
    if (!cmd)
        return;
    record(std::move(cmd));
}

void CommandStack::record(CommandPtr cmd)
{
    const bool wasClean = isClean();
    // Discard the redo tail.
    if (m_index < static_cast<int>(m_commands.size())) {
        m_commands.erase(m_commands.begin() + m_index, m_commands.end());
        if (m_cleanIndex > m_index)
            m_cleanIndex = -1;
    }
    m_commands.push_back(std::move(cmd));
    ++m_index;
    // Enforce the history limit to bound memory usage.
    while (static_cast<int>(m_commands.size()) > m_limit) {
        m_commands.erase(m_commands.begin());
        --m_index;
        m_cleanIndex = m_cleanIndex > 0 ? m_cleanIndex - 1 : -1;
    }
    emitChanges(wasClean);
}

QString CommandStack::undoText() const
{
    return canUndo() ? m_commands[static_cast<size_t>(m_index - 1)]->text() : QString();
}

QString CommandStack::redoText() const
{
    return canRedo() ? m_commands[static_cast<size_t>(m_index)]->text() : QString();
}

void CommandStack::undo()
{
    if (!canUndo())
        return;
    const bool wasClean = isClean();
    Command* cmd = m_commands[static_cast<size_t>(m_index - 1)].get();
    cmd->undo(m_doc);
    --m_index;
    emitChanges(wasClean);
    emit applied(cmd->pageId());
}

void CommandStack::redo()
{
    if (!canRedo())
        return;
    const bool wasClean = isClean();
    Command* cmd = m_commands[static_cast<size_t>(m_index)].get();
    cmd->redo(m_doc);
    ++m_index;
    emitChanges(wasClean);
    emit applied(cmd->pageId());
}

void CommandStack::clear()
{
    const bool wasClean = isClean();
    m_commands.clear();
    m_index = 0;
    m_cleanIndex = 0;
    emitChanges(wasClean);
}

void CommandStack::setClean()
{
    const bool wasClean = isClean();
    m_cleanIndex = m_index;
    emitChanges(wasClean);
}

void CommandStack::setCleanIndex(int index)
{
    const bool wasClean = isClean();
    m_cleanIndex = (index >= 0 && index <= static_cast<int>(m_commands.size())) ? index : -1;
    emitChanges(wasClean);
}

void CommandStack::setLimit(int limit)
{
    m_limit = std::max(10, limit);
}

void CommandStack::emitChanges(bool wasClean)
{
    emit changed();
    if (wasClean != isClean())
        emit cleanChanged(isClean());
}

} // namespace cb
