#include "document/DocumentObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"

#include <QPolygonF>

namespace cb {

namespace {
struct TypeName
{
    ObjectType type;
    const char* name;
};

constexpr TypeName kTypeNames[] = {
    {ObjectType::Stroke, "stroke"},     {ObjectType::Shape, "shape"},
    {ObjectType::Text, "text"},         {ObjectType::Equation, "equation"},
    {ObjectType::Image, "image"},       {ObjectType::Graph, "graph"},
    {ObjectType::Geometry, "geometry"}, {ObjectType::Measurement, "measurement"},
    {ObjectType::Table, "table"},
};
} // namespace

QString objectTypeName(ObjectType type)
{
    for (const auto& entry : kTypeNames)
        if (entry.type == type)
            return QString::fromLatin1(entry.name);
    return QString();
}

bool objectTypeFromName(const QString& name, ObjectType* type)
{
    for (const auto& entry : kTypeNames) {
        if (name == QLatin1String(entry.name)) {
            if (type)
                *type = entry.type;
            return true;
        }
    }
    return false;
}

DocumentObject::DocumentObject(ObjectType type)
    : m_type(type)
    , m_id(newId())
{
}

DocumentObject::~DocumentObject() = default;

void DocumentObject::setPosition(const QPointF& pos)
{
    if (m_position == pos)
        return;
    m_position = pos;
    invalidateBounds();
}

void DocumentObject::setRotation(qreal degrees)
{
    degrees = geom::normalizeDegrees(degrees);
    if (qFuzzyCompare(m_rotation + 1.0, degrees + 1.0))
        return;
    m_rotation = degrees;
    invalidateBounds();
}

QTransform DocumentObject::transform() const
{
    QTransform t;
    t.translate(m_position.x(), m_position.y());
    if (m_rotation != 0.0)
        t.rotate(m_rotation);
    return t;
}

QPointF DocumentObject::mapFromPage(const QPointF& page) const
{
    return geom::rotated(page - m_position, -m_rotation);
}

QRectF DocumentObject::sceneBounds() const
{
    if (!m_boundsValid) {
        const QRectF local = geom::inflated(localBounds(), outlineMargin());
        m_sceneBounds = transform().mapRect(local);
        m_boundsValid = true;
    }
    return m_sceneBounds;
}

bool DocumentObject::hitTest(const QPointF& pagePos, qreal tolerance) const
{
    if (!geom::inflated(sceneBounds(), tolerance).contains(pagePos))
        return false;
    return hitTestLocal(mapFromPage(pagePos), tolerance);
}

bool DocumentObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    return geom::inflated(localBounds(), tolerance + outlineMargin()).contains(local);
}

bool DocumentObject::isInsidePolygon(const QPolygonF& pagePolygon) const
{
    const QRectF b = sceneBounds();
    return pagePolygon.containsPoint(b.center(), Qt::OddEvenFill)
        && pagePolygon.boundingRect().intersects(b);
}

void DocumentObject::resizeTo(const QRectF& newLocalRect, ResizeHint hint)
{
    QRectF target = newLocalRect.normalized();
    if (target.width() < 1.0)
        target.setWidth(1.0);
    if (target.height() < 1.0)
        target.setHeight(1.0);
    const QRectF old = localBounds();
    applyResize(target.size(), hint);
    const QPointF shift = target.center() - old.center();
    m_position += geom::rotated(shift, m_rotation);
    invalidateBounds();
}

void DocumentObject::moveControlPointTo(int index, const QPointF& pagePos)
{
    setControlPoint(index, mapFromPage(pagePos));
    invalidateBounds();
}

bool DocumentObject::setColor(const QColor& color)
{
    Q_UNUSED(color);
    return false;
}

void DocumentObject::recenter()
{
    const QPointF c = localBounds().center();
    if (geom::length(c) < 1e-9)
        return;
    translateContent(-c);
    m_position += geom::rotated(c, m_rotation);
    invalidateBounds();
}

QJsonObject DocumentObject::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("type"), objectTypeName(m_type));
    obj.insert(QStringLiteral("id"), idToString(m_id));
    obj.insert(QStringLiteral("pos"), json::fromPoint(m_position));
    if (m_rotation != 0.0)
        obj.insert(QStringLiteral("rot"), m_rotation);
    writeProperties(obj);
    return obj;
}

bool DocumentObject::fromJson(const QJsonObject& obj)
{
    const QUuid id = idFromString(obj.value(QStringLiteral("id")).toString());
    if (!id.isNull())
        m_id = id;
    m_position = json::toPoint(obj.value(QStringLiteral("pos")));
    m_rotation = obj.value(QStringLiteral("rot")).toDouble(0.0);
    invalidateBounds();
    const bool ok = readProperties(obj);
    invalidateBounds();
    return ok;
}

} // namespace cb
