#include "document/Page.h"

#include "core/JsonUtil.h"
#include "document/ObjectFactory.h"

#include <QJsonArray>

#include <algorithm>

namespace cb {

Page::Page()
    : m_id(newId())
{
}

Page::~Page() = default;

DocumentObject* Page::object(const ObjectId& id) const
{
    const int i = indexOf(id);
    return i >= 0 ? objectAt(i) : nullptr;
}

int Page::indexOf(const ObjectId& id) const
{
    for (size_t i = 0; i < m_objects.size(); ++i)
        if (m_objects[i]->id() == id)
            return static_cast<int>(i);
    return -1;
}

DocumentObject* Page::topmostAt(const QPointF& pagePos, qreal tolerance) const
{
    for (auto it = m_objects.rbegin(); it != m_objects.rend(); ++it)
        if ((*it)->hitTest(pagePos, tolerance))
            return it->get();
    return nullptr;
}

DocumentObject* Page::topmostEnclosing(const QPointF& pagePos) const
{
    for (auto it = m_objects.rbegin(); it != m_objects.rend(); ++it)
        if ((*it)->enclosesPoint(pagePos))
            return it->get();
    return nullptr;
}

std::vector<DocumentObject*> Page::objectsIntersecting(const QRectF& rect) const
{
    std::vector<DocumentObject*> out;
    for (const auto& o : m_objects)
        if (o->sceneBounds().intersects(rect))
            out.push_back(o.get());
    return out;
}

QRectF Page::contentBounds() const
{
    QRectF r;
    for (const auto& o : m_objects)
        r = r.isNull() ? o->sceneBounds() : r.united(o->sceneBounds());
    return r;
}

QRectF Page::exportRect() const
{
    const QRectF frame = frameRect();
    const QRectF content = contentBounds();
    if (content.isNull() || frame.contains(content))
        return frame;
    QRectF r = frame.united(content);
    const qreal aspect = frame.width() / frame.height();
    if (r.width() / r.height() > aspect) {
        const qreal h = r.width() / aspect;
        r.adjust(0, -(h - r.height()) / 2, 0, (h - r.height()) / 2);
    } else {
        const qreal w = r.height() * aspect;
        r.adjust(-(w - r.width()) / 2, 0, (w - r.width()) / 2, 0);
    }
    return r;
}

void Page::insertObject(int index, ObjectPtr object)
{
    index = std::clamp(index, 0, objectCount());
    m_objects.insert(m_objects.begin() + index, std::move(object));
    touch();
}

ObjectPtr Page::takeObject(int index)
{
    if (index < 0 || index >= objectCount())
        return nullptr;
    ObjectPtr o = std::move(m_objects[static_cast<size_t>(index)]);
    m_objects.erase(m_objects.begin() + index);
    touch();
    return o;
}

ObjectPtr Page::replaceObject(int index, ObjectPtr object)
{
    if (index < 0 || index >= objectCount())
        return nullptr;
    std::swap(m_objects[static_cast<size_t>(index)], object);
    touch();
    return object;
}

std::unique_ptr<Page> Page::clone(bool freshIds) const
{
    auto p = std::make_unique<Page>();
    p->m_id = freshIds ? newId() : m_id;
    p->m_name = m_name;
    p->m_template = m_template;
    p->m_size = m_size;
    p->m_objects.reserve(m_objects.size());
    for (const auto& o : m_objects) {
        ObjectPtr c = o->clone();
        if (freshIds)
            c->setId(newId());
        p->m_objects.push_back(std::move(c));
    }
    return p;
}

QJsonObject Page::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), idToString(m_id));
    if (!m_name.isEmpty())
        o.insert(QStringLiteral("name"), m_name);
    o.insert(QStringLiteral("size"), json::fromSize(m_size));
    o.insert(QStringLiteral("template"), m_template.toJson());
    QJsonArray objects;
    for (const auto& obj : m_objects)
        objects.append(obj->toJson());
    o.insert(QStringLiteral("objects"), objects);
    return o;
}

std::unique_ptr<Page> Page::fromJson(const QJsonObject& obj, int* skipped)
{
    auto p = std::make_unique<Page>();
    const QUuid id = idFromString(obj.value(QStringLiteral("id")).toString());
    if (!id.isNull())
        p->m_id = id;
    p->m_name = obj.value(QStringLiteral("name")).toString();
    const QSizeF size = json::toSize(obj.value(QStringLiteral("size")), QSizeF(kDefaultWidth, kDefaultHeight));
    if (size.width() >= 100 && size.height() >= 100)
        p->m_size = size;
    p->m_template = TemplateSpec::fromJson(obj.value(QStringLiteral("template")).toObject());
    const QJsonArray objects = obj.value(QStringLiteral("objects")).toArray();
    int skippedCount = 0;
    for (const QJsonValue& v : objects) {
        ObjectPtr o = ObjectFactory::fromJson(v.toObject());
        if (o)
            p->m_objects.push_back(std::move(o));
        else
            ++skippedCount;
    }
    if (skipped)
        *skipped += skippedCount;
    return p;
}

} // namespace cb
