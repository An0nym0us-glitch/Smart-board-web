#pragma once

#include "document/Command.h"
#include "document/ImageStore.h"
#include "document/Page.h"
#include "math/CoordinateSystem.h"

#include <QDateTime>
#include <QObject>

#include <memory>
#include <vector>

namespace cb {

struct DocumentMetadata
{
    QString title;
    QString author;
    QDateTime created = QDateTime::currentDateTimeUtc();
    QDateTime modified = QDateTime::currentDateTimeUtc();
};

/// Immutable deep copy of (part of) a document for background work such as export.
struct DocumentSnapshot
{
    std::vector<PagePtr> pages;
    ImageStore images;
    CoordinateSystem coordinates;
    DocumentMetadata metadata;
};

/// Everything needed to (re)build a document, produced by deserialisation.
struct DocumentContents
{
    std::vector<PagePtr> pages;
    ImageStore images;
    CoordinateSystem coordinates;
    DocumentMetadata metadata;
    TemplateSpec defaultTemplate;
    QSizeF defaultPageSize{Page::kDefaultWidth, Page::kDefaultHeight};
    int currentPage = 0;
};

/// A lesson: an ordered list of pages plus shared assets and a single undo history.
class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);
    ~Document() override;

    /// Replaces the document by a single empty page.
    void resetToNew(const TemplateSpec& background);
    /// Replaces the whole content (e.g. after loading a file).
    void setContents(DocumentContents contents);
    /// Extracts a copy of the whole document content (used for saving on another thread).
    DocumentContents copyContents() const;

    int pageCount() const { return static_cast<int>(m_pages.size()); }
    Page* page(int index) const;
    Page* pageById(const PageId& id) const;
    int indexOfPage(const PageId& id) const;

    int currentPageIndex() const { return m_currentPage; }
    Page* currentPage() const { return page(m_currentPage); }
    void setCurrentPageIndex(int index);

    // ---- Low-level mutators, used by commands only ----
    void insertPage(int index, PagePtr page);
    PagePtr takePage(int index);
    void movePage(int from, int to);
    void insertObject(const PageId& pageId, int index, ObjectPtr object);
    ObjectPtr takeObject(const PageId& pageId, const ObjectId& objectId, int* index = nullptr);
    /// Replaces an object (same id) and returns the previous instance.
    ObjectPtr replaceObject(const PageId& pageId, ObjectPtr object);
    void moveObject(const PageId& pageId, const ObjectId& objectId, int newIndex);
    /// Notifies that an object was mutated in place; oldBounds is its scene rect before the change.
    void notifyObjectChanged(const PageId& pageId, const ObjectId& objectId, const QRectF& oldBounds);
    /// Notifies that page properties (name, background) changed.
    void notifyPageChanged(const PageId& pageId);

    CommandStack& commands() { return *m_commands; }
    const CommandStack& commands() const { return *m_commands; }

    ImageStore& images() { return m_images; }
    const ImageStore& images() const { return m_images; }

    CoordinateSystem& coordinates() { return m_coordinates; }
    const CoordinateSystem& coordinates() const { return m_coordinates; }
    /// The global system as it applies to a page (origin at the page centre, shared units/scale).
    CoordinateSystem coordinatesFor(const Page* page) const;
    /// Changes the drawing scale (used by SetScaleCommand).
    void setScale(const MeasureScale& scale);

    DocumentMetadata& metadata() { return m_metadata; }
    const DocumentMetadata& metadata() const { return m_metadata; }

    const TemplateSpec& defaultTemplate() const { return m_defaultTemplate; }
    void setDefaultTemplate(const TemplateSpec& spec) { m_defaultTemplate = spec; }
    /// Logical size of new pages (document units, see PageSize.h).
    QSizeF defaultPageSize() const { return m_defaultPageSize; }
    void setDefaultPageSize(const QSizeF& size) { m_defaultPageSize = size; }

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString& path);
    QString displayName() const;

    bool isModified() const;
    /// Marks the current state as saved.
    void markSaved();

    /// Incremented on every content change (autosave, thumbnails).
    quint64 revision() const { return m_revision; }

    /// Deep copy of selected pages (all if empty) for background processing.
    std::unique_ptr<DocumentSnapshot> snapshot(const QVector<int>& pageIndices = {}) const;

    /// Creates a blank page using the default template.
    PagePtr createPage() const;

signals:
    void pagesChanged();
    void currentPageChanged(int index);
    void objectAdded(const QUuid& pageId, const QUuid& objectId, const QRectF& bounds);
    void objectRemoved(const QUuid& pageId, const QUuid& objectId, const QRectF& bounds);
    void objectChanged(const QUuid& pageId, const QUuid& objectId, const QRectF& oldBounds, const QRectF& newBounds);
    void pageChanged(const QUuid& pageId);
    void contentChanged();
    void modifiedChanged(bool modified);
    void filePathChanged(const QString& path);
    /// Units or drawing scale changed: every measurement label must be redrawn.
    void coordinatesChanged();
    /// Emitted when the entire content was replaced (new/open).
    void documentReset();

private:
    void bump();

    std::vector<PagePtr> m_pages;
    int m_currentPage = 0;
    std::unique_ptr<CommandStack> m_commands;
    ImageStore m_images;
    CoordinateSystem m_coordinates;
    DocumentMetadata m_metadata;
    TemplateSpec m_defaultTemplate;
    QSizeF m_defaultPageSize{Page::kDefaultWidth, Page::kDefaultHeight};
    QString m_filePath;
    quint64 m_revision = 0;
};

} // namespace cb
