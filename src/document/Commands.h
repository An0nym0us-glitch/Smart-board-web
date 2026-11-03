#pragma once

#include "document/Command.h"
#include "document/DocumentObject.h"
#include "document/Page.h"
#include "math/MeasureScale.h"

#include <vector>

namespace cb {

/// Adds objects to a page (at the top of the z-order unless indices are given).
class AddObjectsCommand final : public Command
{
public:
    AddObjectsCommand(const PageId& page, std::vector<ObjectPtr> objects, QString text = QString());
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }
    std::vector<ObjectId> objectIds() const;

private:
    struct Entry
    {
        ObjectId id;
        int index = -1;
        ObjectPtr object;
    };
    PageId m_page;
    std::vector<Entry> m_entries;
    QString m_text;
};

/// Removes objects from a page.
class RemoveObjectsCommand final : public Command
{
public:
    RemoveObjectsCommand(const PageId& page, std::vector<ObjectId> ids, QString text = QString());
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }

private:
    struct Entry
    {
        ObjectId id;
        int index = -1;
        ObjectPtr object;
    };
    PageId m_page;
    std::vector<ObjectId> m_ids;
    std::vector<Entry> m_removed;
    QString m_text;
};

/// Replaces object states (before/after snapshots). Used for move, resize, rotate, recolour, edits.
class ModifyObjectsCommand final : public Command
{
public:
    explicit ModifyObjectsCommand(const PageId& page, QString text = QString());
    /// Adds a before/after pair (both clones with the same id).
    void add(ObjectPtr before, ObjectPtr after);
    bool isEmpty() const { return m_entries.empty(); }
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }

private:
    struct Entry
    {
        ObjectPtr before;
        ObjectPtr after;
    };
    PageId m_page;
    std::vector<Entry> m_entries;
    QString m_text;
};

/// Replaces a set of objects by another set (e.g. the eraser splitting strokes).
///
/// Created after the change has been applied: `removed` holds the original objects with their
/// indices before the change, `added` references the new objects (living in the page) with their
/// final indices.
class ReplaceObjectsCommand final : public Command
{
public:
    struct Removed
    {
        int index = -1;
        ObjectPtr object;
    };
    struct Added
    {
        ObjectId id;
        int index = -1;
    };
    ReplaceObjectsCommand(const PageId& page, std::vector<Removed> removed, std::vector<Added> added, QString text);
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }

private:
    struct Slot
    {
        ObjectId id;
        int index = -1;
        ObjectPtr object; // owned while outside the document
    };
    PageId m_page;
    std::vector<Slot> m_removed; // ascending original index
    std::vector<Slot> m_added;   // ascending final index
    QString m_text;
};

/// Changes the z-order of an object.
class ReorderObjectCommand final : public Command
{
public:
    ReorderObjectCommand(const PageId& page, const ObjectId& id, int from, int to);
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override;
    PageId pageId() const override { return m_page; }

private:
    PageId m_page;
    ObjectId m_id;
    int m_from;
    int m_to;
};

/// Inserts a page and navigates to it.
class InsertPageCommand final : public Command
{
public:
    InsertPageCommand(int index, PagePtr page, QString text = QString());
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_pageId; }

private:
    int m_index;
    PageId m_pageId;
    PagePtr m_page;
    int m_previousCurrent = 0;
    QString m_text;
};

/// Removes a page.
class RemovePageCommand final : public Command
{
public:
    explicit RemovePageCommand(int index);
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override;

private:
    int m_index;
    PagePtr m_page;
};

/// Moves a page within the lesson.
class MovePageCommand final : public Command
{
public:
    MovePageCommand(int from, int to);
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override;

private:
    int m_from;
    int m_to;
};

/// Changes page properties (name and/or background template).
class ModifyPageCommand final : public Command
{
public:
    ModifyPageCommand(const PageId& page, const QString& name, const TemplateSpec& background, QString text);
    void redo(Document& doc) override;
    void undo(Document& doc) override;
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }

private:
    void apply(Document& doc);
    PageId m_page;
    QString m_name;
    TemplateSpec m_background;
    QString m_text;
};

/// Changes the logical size of a page (content keeps its document coordinates).
class SetPageSizeCommand final : public Command
{
public:
    SetPageSizeCommand(const PageId& page, const QSizeF& size, QString text);
    void redo(Document& doc) override { apply(doc); }
    void undo(Document& doc) override { apply(doc); }
    QString text() const override { return m_text; }
    PageId pageId() const override { return m_page; }

private:
    void apply(Document& doc);
    PageId m_page;
    QSizeF m_size;
    QString m_text;
};

/// Changes the lesson's drawing scale (e.g. 10 cm = 1 km).
class SetScaleCommand final : public Command
{
public:
    SetScaleCommand(const MeasureScale& scale, QString text);
    void redo(Document& doc) override { apply(doc); }
    void undo(Document& doc) override { apply(doc); }
    QString text() const override { return m_text; }

private:
    void apply(Document& doc);
    MeasureScale m_scale;
    QString m_text;
};

} // namespace cb
