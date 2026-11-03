#pragma once

#include "document/DocumentObject.h"
#include "document/PageTemplate.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>

#include <memory>
#include <vector>

namespace cb {

/// A page (slide) of a lesson: an ordered list of objects (z-order) on a template background.
///
/// Pages are infinite canvases; frameRect() defines the nominal 16:9 area used for export,
/// thumbnails and "fit" navigation.
class Page
{
public:
    static constexpr qreal kDefaultWidth = 1920.0;
    static constexpr qreal kDefaultHeight = 1080.0;

    Page();
    ~Page();

    const PageId& id() const { return m_id; }
    void setId(const PageId& id) { m_id = id; }

    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    const TemplateSpec& background() const { return m_template; }
    void setBackground(const TemplateSpec& spec) { m_template = spec; }

    QRectF frameRect() const { return QRectF(QPointF(0, 0), m_size); }
    QSizeF size() const { return m_size; }
    void setSize(const QSizeF& size) { m_size = size; }

    int objectCount() const { return static_cast<int>(m_objects.size()); }
    DocumentObject* objectAt(int index) const { return m_objects[static_cast<size_t>(index)].get(); }
    DocumentObject* object(const ObjectId& id) const;
    int indexOf(const ObjectId& id) const;
    const std::vector<ObjectPtr>& objects() const { return m_objects; }

    /// Topmost object hit at a page position.
    DocumentObject* topmostAt(const QPointF& pagePos, qreal tolerance) const;
    /// Topmost object whose closed outline encloses the position (unfilled shapes).
    DocumentObject* topmostEnclosing(const QPointF& pagePos) const;
    /// Objects whose bounds intersect the rect, in z-order.
    std::vector<DocumentObject*> objectsIntersecting(const QRectF& rect) const;

    /// Bounds of all content (empty rect if the page is empty).
    QRectF contentBounds() const;

    /// Export area: the frame, grown (keeping aspect ratio) to include all content.
    QRectF exportRect() const;

    /// Monotonic change counter used by thumbnail caches.
    quint64 revision() const { return m_revision; }
    void touch() { ++m_revision; }

    // Low-level mutators. Use Document methods so that signals are emitted.
    void insertObject(int index, ObjectPtr object);
    ObjectPtr takeObject(int index);
    ObjectPtr replaceObject(int index, ObjectPtr object);

    /// Deep copy. If freshIds is set, the page and all objects receive new ids.
    std::unique_ptr<Page> clone(bool freshIds) const;

    QJsonObject toJson() const;
    /// Parses a page. Unknown object types are skipped (and counted in *skipped).
    static std::unique_ptr<Page> fromJson(const QJsonObject& obj, int* skipped = nullptr);

private:
    PageId m_id;
    QString m_name;
    TemplateSpec m_template;
    QSizeF m_size{kDefaultWidth, kDefaultHeight};
    std::vector<ObjectPtr> m_objects;
    quint64 m_revision = 0;
};

using PagePtr = std::unique_ptr<Page>;

} // namespace cb
