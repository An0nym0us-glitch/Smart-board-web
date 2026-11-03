#include "document/Document.h"

#include "document/CoordinateResolver.h"

#include <QFileInfo>

#include <algorithm>

namespace cb {

namespace {
CoordinateSystem defaultCoordinates()
{
    return CoordinateSystem::global(QPointF(Page::kDefaultWidth / 2, Page::kDefaultHeight / 2), 40.0,
                                    QStringLiteral("cm"));
}
} // namespace

Document::Document(QObject* parent)
    : QObject(parent)
    , m_commands(std::make_unique<CommandStack>(*this))
    , m_coordinates(defaultCoordinates())
{
    connect(m_commands.get(), &CommandStack::cleanChanged, this, [this](bool clean) {
        emit modifiedChanged(!clean);
    });
    m_pages.push_back(createPage());
}

Document::~Document()
{
    // Destroy the command stack before pages: commands may own objects.
    m_commands.reset();
}

PagePtr Document::createPage() const
{
    auto p = std::make_unique<Page>();
    p->setBackground(m_defaultTemplate);
    p->setSize(m_defaultPageSize);
    return p;
}

CoordinateSystem Document::coordinatesFor(const Page* page) const
{
    return pageCoordinateSystem(m_coordinates, page);
}

void Document::setScale(const MeasureScale& scale)
{
    m_coordinates.setScale(scale);
    for (auto& p : m_pages)
        p->touch();
    bump();
    emit coordinatesChanged();
}

void Document::resetToNew(const TemplateSpec& background)
{
    m_commands->clear();
    m_pages.clear();
    m_images.clear();
    m_defaultTemplate = background;
    m_defaultPageSize = QSizeF(Page::kDefaultWidth, Page::kDefaultHeight);
    m_coordinates = defaultCoordinates();
    m_metadata = DocumentMetadata();
    m_pages.push_back(createPage());
    m_currentPage = 0;
    setFilePath(QString());
    bump();
    emit documentReset();
    emit pagesChanged();
    emit currentPageChanged(0);
    emit modifiedChanged(false);
}

void Document::setContents(DocumentContents contents)
{
    m_commands->clear();
    m_pages = std::move(contents.pages);
    if (m_pages.empty())
        m_pages.push_back(createPage());
    m_images = std::move(contents.images);
    m_coordinates = contents.coordinates;
    m_metadata = contents.metadata;
    m_defaultTemplate = contents.defaultTemplate;
    m_defaultPageSize = contents.defaultPageSize;
    m_currentPage = std::clamp(contents.currentPage, 0, pageCount() - 1);
    bump();
    emit documentReset();
    emit pagesChanged();
    emit currentPageChanged(m_currentPage);
    emit modifiedChanged(false);
}

DocumentContents Document::copyContents() const
{
    DocumentContents c;
    c.pages.reserve(m_pages.size());
    for (const auto& p : m_pages)
        c.pages.push_back(p->clone(false));
    c.images = m_images;
    c.coordinates = m_coordinates;
    c.metadata = m_metadata;
    c.defaultTemplate = m_defaultTemplate;
    c.defaultPageSize = m_defaultPageSize;
    c.currentPage = m_currentPage;
    return c;
}

Page* Document::page(int index) const
{
    if (index < 0 || index >= pageCount())
        return nullptr;
    return m_pages[static_cast<size_t>(index)].get();
}

Page* Document::pageById(const PageId& id) const
{
    const int i = indexOfPage(id);
    return i >= 0 ? page(i) : nullptr;
}

int Document::indexOfPage(const PageId& id) const
{
    for (size_t i = 0; i < m_pages.size(); ++i)
        if (m_pages[i]->id() == id)
            return static_cast<int>(i);
    return -1;
}

void Document::setCurrentPageIndex(int index)
{
    index = std::clamp(index, 0, pageCount() - 1);
    if (index == m_currentPage)
        return;
    m_currentPage = index;
    emit currentPageChanged(index);
}

void Document::insertPage(int index, PagePtr page)
{
    if (!page)
        return;
    index = std::clamp(index, 0, pageCount());
    m_pages.insert(m_pages.begin() + index, std::move(page));
    if (index <= m_currentPage && pageCount() > 1)
        ++m_currentPage;
    bump();
    emit pagesChanged();
    emit currentPageChanged(m_currentPage);
}

PagePtr Document::takePage(int index)
{
    if (index < 0 || index >= pageCount() || pageCount() <= 1)
        return nullptr;
    PagePtr p = std::move(m_pages[static_cast<size_t>(index)]);
    m_pages.erase(m_pages.begin() + index);
    if (index < m_currentPage || m_currentPage >= pageCount())
        m_currentPage = std::max(0, m_currentPage - 1);
    bump();
    emit pagesChanged();
    emit currentPageChanged(m_currentPage);
    return p;
}

void Document::movePage(int from, int to)
{
    if (from < 0 || from >= pageCount() || to < 0 || to >= pageCount() || from == to)
        return;
    const PageId current = currentPage()->id();
    PagePtr p = std::move(m_pages[static_cast<size_t>(from)]);
    m_pages.erase(m_pages.begin() + from);
    m_pages.insert(m_pages.begin() + to, std::move(p));
    m_currentPage = indexOfPage(current);
    bump();
    emit pagesChanged();
    emit currentPageChanged(m_currentPage);
}

void Document::insertObject(const PageId& pageId, int index, ObjectPtr object)
{
    Page* p = pageById(pageId);
    if (!p || !object)
        return;
    const ObjectId id = object->id();
    const QRectF bounds = object->sceneBounds();
    p->insertObject(index, std::move(object));
    bump();
    emit objectAdded(pageId, id, bounds);
}

ObjectPtr Document::takeObject(const PageId& pageId, const ObjectId& objectId, int* index)
{
    Page* p = pageById(pageId);
    if (!p)
        return nullptr;
    const int i = p->indexOf(objectId);
    if (i < 0)
        return nullptr;
    if (index)
        *index = i;
    ObjectPtr o = p->takeObject(i);
    bump();
    emit objectRemoved(pageId, objectId, o->sceneBounds());
    return o;
}

ObjectPtr Document::replaceObject(const PageId& pageId, ObjectPtr object)
{
    Page* p = pageById(pageId);
    if (!p || !object)
        return nullptr;
    const int i = p->indexOf(object->id());
    if (i < 0)
        return nullptr;
    const ObjectId id = object->id();
    const QRectF newBounds = object->sceneBounds();
    ObjectPtr old = p->replaceObject(i, std::move(object));
    bump();
    emit objectChanged(pageId, id, old->sceneBounds(), newBounds);
    return old;
}

void Document::moveObject(const PageId& pageId, const ObjectId& objectId, int newIndex)
{
    Page* p = pageById(pageId);
    if (!p)
        return;
    const int i = p->indexOf(objectId);
    if (i < 0)
        return;
    ObjectPtr o = p->takeObject(i);
    const QRectF bounds = o->sceneBounds();
    p->insertObject(newIndex, std::move(o));
    bump();
    emit objectChanged(pageId, objectId, bounds, bounds);
}

void Document::notifyObjectChanged(const PageId& pageId, const ObjectId& objectId, const QRectF& oldBounds)
{
    Page* p = pageById(pageId);
    if (!p)
        return;
    DocumentObject* o = p->object(objectId);
    if (!o)
        return;
    p->touch();
    bump();
    emit objectChanged(pageId, objectId, oldBounds, o->sceneBounds());
}

void Document::notifyPageChanged(const PageId& pageId)
{
    if (Page* p = pageById(pageId))
        p->touch();
    bump();
    emit pageChanged(pageId);
}

void Document::setFilePath(const QString& path)
{
    if (m_filePath == path)
        return;
    m_filePath = path;
    emit filePathChanged(path);
}

QString Document::displayName() const
{
    if (!m_metadata.title.isEmpty())
        return m_metadata.title;
    if (!m_filePath.isEmpty())
        return QFileInfo(m_filePath).completeBaseName();
    return tr("Untitled lesson");
}

bool Document::isModified() const
{
    return !m_commands->isClean();
}

void Document::markSaved()
{
    m_commands->setClean();
}

std::unique_ptr<DocumentSnapshot> Document::snapshot(const QVector<int>& pageIndices) const
{
    auto snap = std::make_unique<DocumentSnapshot>();
    if (pageIndices.isEmpty()) {
        for (const auto& p : m_pages)
            snap->pages.push_back(p->clone(false));
    } else {
        for (int i : pageIndices)
            if (const Page* p = page(i))
                snap->pages.push_back(p->clone(false));
    }
    snap->images = m_images;
    snap->coordinates = m_coordinates;
    snap->metadata = m_metadata;
    return snap;
}

void Document::bump()
{
    ++m_revision;
    emit contentChanged();
}

} // namespace cb
