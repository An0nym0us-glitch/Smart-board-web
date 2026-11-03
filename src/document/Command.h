#pragma once

#include "core/Id.h"

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

namespace cb {

class Document;

/// A reversible document operation. All document edits go through commands so that there is a
/// single undo/redo history for every tool.
class Command
{
public:
    virtual ~Command() = default;
    virtual void redo(Document& doc) = 0;
    virtual void undo(Document& doc) = 0;
    virtual QString text() const = 0;
    /// The page the command affects (used to navigate there on undo/redo). Null for document-wide.
    virtual PageId pageId() const { return PageId(); }
};

using CommandPtr = std::unique_ptr<Command>;

/// Groups several commands into one undo step.
class CompositeCommand final : public Command
{
public:
    explicit CompositeCommand(QString text);
    void add(CommandPtr cmd);
    bool isEmpty() const { return m_children.empty(); }
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override;

private:
    QString m_text;
    std::vector<CommandPtr> m_children;
};

/// Linear undo/redo history with clean-state tracking and a bounded size.
class CommandStack : public QObject
{
    Q_OBJECT
public:
    explicit CommandStack(Document& doc, QObject* parent = nullptr);
    ~CommandStack() override;

    /// Executes the command and records it.
    void push(CommandPtr cmd);
    /// Records a command whose effect has already been applied (interactive edits).
    void pushApplied(CommandPtr cmd);

    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < static_cast<int>(m_commands.size()); }
    QString undoText() const;
    QString redoText() const;
    void undo();
    void redo();

    void clear();
    void setClean();
    /// Marks an earlier/later history position as the saved state (after an asynchronous save).
    void setCleanIndex(int index);
    bool isClean() const { return m_index == m_cleanIndex; }
    int count() const { return static_cast<int>(m_commands.size()); }
    int index() const { return m_index; }

    void setLimit(int limit);
    int limit() const { return m_limit; }

signals:
    void changed();
    void cleanChanged(bool clean);
    /// Emitted after undo/redo with the page affected (may be null).
    void applied(const QUuid& pageId);

private:
    void record(CommandPtr cmd);
    void emitChanges(bool wasClean);

    Document& m_doc;
    std::vector<CommandPtr> m_commands;
    int m_index = 0;
    int m_cleanIndex = 0;
    int m_limit = 300;
};

} // namespace cb
