#include "document/Commands.h"

#include "document/Document.h"

#include <QCoreApplication>

#include <algorithm>

namespace cb {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("Commands", s); }
} // namespace

// ---------------------------------------------------------------- AddObjectsCommand

AddObjectsCommand::AddObjectsCommand(const PageId& page, std::vector<ObjectPtr> objects, QString text)
    : m_page(page)
    , m_text(text.isEmpty() ? tr("Add") : std::move(text))
{
    for (auto& o : objects) {
        Entry e;
        e.id = o->id();
        e.object = std::move(o);
        m_entries.push_back(std::move(e));
    }
}

std::vector<ObjectId> AddObjectsCommand::objectIds() const
{
    std::vector<ObjectId> ids;
    for (const Entry& e : m_entries)
        ids.push_back(e.id);
    return ids;
}

void AddObjectsCommand::redo(Document& doc)
{
    Page* p = doc.pageById(m_page);
    if (!p)
        return;
    for (Entry& e : m_entries) {
        if (!e.object)
            continue;
        const int index = e.index >= 0 ? e.index : p->objectCount();
        e.index = index;
        doc.insertObject(m_page, index, std::move(e.object));
    }
}

void AddObjectsCommand::undo(Document& doc)
{
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        it->object = doc.takeObject(m_page, it->id);
}

// ---------------------------------------------------------------- RemoveObjectsCommand

RemoveObjectsCommand::RemoveObjectsCommand(const PageId& page, std::vector<ObjectId> ids, QString text)
    : m_page(page)
    , m_ids(std::move(ids))
    , m_text(text.isEmpty() ? tr("Delete") : std::move(text))
{
}

void RemoveObjectsCommand::redo(Document& doc)
{
    m_removed.clear();
    for (const ObjectId& id : m_ids) {
        Entry e;
        e.id = id;
        e.object = doc.takeObject(m_page, id, &e.index);
        if (e.object)
            m_removed.push_back(std::move(e));
    }
}

void RemoveObjectsCommand::undo(Document& doc)
{
    // Reinsert in reverse removal order so every index refers to the state it was taken from.
    for (auto it = m_removed.rbegin(); it != m_removed.rend(); ++it)
        doc.insertObject(m_page, it->index, std::move(it->object));
    m_removed.clear();
}

// ---------------------------------------------------------------- ModifyObjectsCommand

ModifyObjectsCommand::ModifyObjectsCommand(const PageId& page, QString text)
    : m_page(page)
    , m_text(text.isEmpty() ? tr("Edit") : std::move(text))
{
}

void ModifyObjectsCommand::add(ObjectPtr before, ObjectPtr after)
{
    if (!before || !after)
        return;
    m_entries.push_back({std::move(before), std::move(after)});
}

void ModifyObjectsCommand::redo(Document& doc)
{
    for (Entry& e : m_entries)
        doc.replaceObject(m_page, e.after->clone());
}

void ModifyObjectsCommand::undo(Document& doc)
{
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it)
        doc.replaceObject(m_page, it->before->clone());
}

// ---------------------------------------------------------------- ReplaceObjectsCommand

ReplaceObjectsCommand::ReplaceObjectsCommand(const PageId& page, std::vector<Removed> removed,
                                             std::vector<Added> added, QString text)
    : m_page(page)
    , m_text(std::move(text))
{
    for (Removed& r : removed) {
        Slot s;
        s.id = r.object->id();
        s.index = r.index;
        s.object = std::move(r.object);
        m_removed.push_back(std::move(s));
    }
    for (const Added& a : added) {
        Slot s;
        s.id = a.id;
        s.index = a.index;
        m_added.push_back(std::move(s));
    }
    auto byIndex = [](const Slot& a, const Slot& b) { return a.index < b.index; };
    std::sort(m_removed.begin(), m_removed.end(), byIndex);
    std::sort(m_added.begin(), m_added.end(), byIndex);
}

void ReplaceObjectsCommand::undo(Document& doc)
{
    for (Slot& s : m_added)
        s.object = doc.takeObject(m_page, s.id);
    // Ascending original indices: every lower original is already back in place.
    for (Slot& s : m_removed)
        if (s.object)
            doc.insertObject(m_page, s.index, std::move(s.object));
}

void ReplaceObjectsCommand::redo(Document& doc)
{
    for (Slot& s : m_removed)
        s.object = doc.takeObject(m_page, s.id);
    for (Slot& s : m_added)
        if (s.object)
            doc.insertObject(m_page, s.index, std::move(s.object));
}

// ---------------------------------------------------------------- ReorderObjectCommand

ReorderObjectCommand::ReorderObjectCommand(const PageId& page, const ObjectId& id, int from, int to)
    : m_page(page)
    , m_id(id)
    , m_from(from)
    , m_to(to)
{
}

void ReorderObjectCommand::redo(Document& doc) { doc.moveObject(m_page, m_id, m_to); }
void ReorderObjectCommand::undo(Document& doc) { doc.moveObject(m_page, m_id, m_from); }
QString ReorderObjectCommand::text() const { return tr("Arrange"); }

// ---------------------------------------------------------------- InsertPageCommand

InsertPageCommand::InsertPageCommand(int index, PagePtr page, QString text)
    : m_index(index)
    , m_pageId(page->id())
    , m_page(std::move(page))
    , m_text(text.isEmpty() ? tr("New page") : std::move(text))
{
}

void InsertPageCommand::redo(Document& doc)
{
    m_previousCurrent = doc.currentPageIndex();
    doc.insertPage(m_index, std::move(m_page));
    doc.setCurrentPageIndex(doc.indexOfPage(m_pageId));
}

void InsertPageCommand::undo(Document& doc)
{
    const int i = doc.indexOfPage(m_pageId);
    m_page = doc.takePage(i);
    doc.setCurrentPageIndex(m_previousCurrent);
}

// ---------------------------------------------------------------- RemovePageCommand

RemovePageCommand::RemovePageCommand(int index)
    : m_index(index)
{
}

void RemovePageCommand::redo(Document& doc)
{
    m_page = doc.takePage(m_index);
}

void RemovePageCommand::undo(Document& doc)
{
    if (!m_page)
        return;
    doc.insertPage(m_index, std::move(m_page));
    doc.setCurrentPageIndex(m_index);
}

QString RemovePageCommand::text() const { return tr("Delete page"); }

// ---------------------------------------------------------------- MovePageCommand

MovePageCommand::MovePageCommand(int from, int to)
    : m_from(from)
    , m_to(to)
{
}

void MovePageCommand::redo(Document& doc) { doc.movePage(m_from, m_to); }
void MovePageCommand::undo(Document& doc) { doc.movePage(m_to, m_from); }
QString MovePageCommand::text() const { return tr("Move page"); }

// ---------------------------------------------------------------- ModifyPageCommand

ModifyPageCommand::ModifyPageCommand(const PageId& page, const QString& name, const TemplateSpec& background,
                                     QString text)
    : m_page(page)
    , m_name(name)
    , m_background(background)
    , m_text(std::move(text))
{
}

void ModifyPageCommand::apply(Document& doc)
{
    Page* p = doc.pageById(m_page);
    if (!p)
        return;
    const QString oldName = p->name();
    const TemplateSpec oldBackground = p->background();
    p->setName(m_name);
    p->setBackground(m_background);
    m_name = oldName;
    m_background = oldBackground;
    doc.notifyPageChanged(m_page);
}

void ModifyPageCommand::redo(Document& doc) { apply(doc); }
void ModifyPageCommand::undo(Document& doc) { apply(doc); }

// ---------------------------------------------------------------- SetPageSizeCommand

SetPageSizeCommand::SetPageSizeCommand(const PageId& page, const QSizeF& size, QString text)
    : m_page(page)
    , m_size(size)
    , m_text(std::move(text))
{
}

void SetPageSizeCommand::apply(Document& doc)
{
    Page* p = doc.pageById(m_page);
    if (!p)
        return;
    const QSizeF old = p->size();
    p->setSize(m_size);
    p->touch();
    m_size = old;
    doc.notifyPageChanged(m_page);
}

// ------------------------------------------------------------------- SetScaleCommand

SetScaleCommand::SetScaleCommand(const MeasureScale& scale, QString text)
    : m_scale(scale)
    , m_text(std::move(text))
{
}

void SetScaleCommand::apply(Document& doc)
{
    const MeasureScale old = doc.coordinates().scale();
    doc.setScale(m_scale);
    m_scale = old;
}

} // namespace cb
