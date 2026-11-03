#include "geometry/GeometryObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"
#include "document/CoordinateResolver.h"
#include "document/ShapeObject.h"
#include "geometry/Label.h"
#include "math/CoordinateSystem.h"

#include <QPainter>

namespace cb {

QString constructKindName(ConstructKind kind)
{
    switch (kind) {
    case ConstructKind::Point: return QStringLiteral("point");
    case ConstructKind::Segment: return QStringLiteral("segment");
    case ConstructKind::Line: return QStringLiteral("line");
    case ConstructKind::Ray: return QStringLiteral("ray");
    case ConstructKind::Vector: return QStringLiteral("vector");
    }
    return QStringLiteral("point");
}

ConstructKind constructKindFromName(const QString& name)
{
    if (name == QLatin1String("segment"))
        return ConstructKind::Segment;
    if (name == QLatin1String("line"))
        return ConstructKind::Line;
    if (name == QLatin1String("ray"))
        return ConstructKind::Ray;
    if (name == QLatin1String("vector"))
        return ConstructKind::Vector;
    return ConstructKind::Point;
}

GeometryObject::GeometryObject()
    : DocumentObject(ObjectType::Geometry)
{
}

std::unique_ptr<GeometryObject> GeometryObject::create(ConstructKind kind, const QVector<QPointF>& pagePoints,
                                                       const QColor& color, bool showLabel)
{
    auto g = std::make_unique<GeometryObject>();
    g->m_kind = kind;
    g->m_color = color;
    g->m_showLabel = showLabel;
    const QPointF c = geom::boundingRect(pagePoints).center();
    g->setPosition(c);
    for (const QPointF& p : pagePoints)
        g->m_points.push_back(p - c);
    return g;
}

QVector<QPointF> GeometryObject::pagePoints() const
{
    QVector<QPointF> out;
    for (const QPointF& p : m_points)
        out.push_back(mapToPage(p));
    return out;
}

void GeometryObject::drawnSegment(ConstructKind kind, const QVector<QPointF>& pts, QPointF& a, QPointF& b)
{
    a = pts.value(0);
    b = pts.value(1, a);
    const QPointF dir = geom::normalized(b - a);
    if (kind == ConstructKind::Line) {
        a -= dir * kLineExtent;
        b += dir * kLineExtent;
    } else if (kind == ConstructKind::Ray) {
        b += dir * kLineExtent;
    }
}

QRectF GeometryObject::localBounds() const
{
    if (m_kind == ConstructKind::Line || m_kind == ConstructKind::Ray) {
        QPointF a, b;
        drawnSegment(m_kind, m_points, a, b);
        return QRectF(a, b).normalized().united(geom::boundingRect(m_points));
    }
    return geom::boundingRect(m_points);
}

QString GeometryObject::format(ConstructKind kind, const QVector<QPointF>& pts, const QString& name,
                               const CoordinateSystem& cs)
{
    const QString prefix = name.isEmpty() ? QString() : name + QLatin1Char(' ');
    auto coords = [&](const QPointF& page) {
        const QPointF m = cs.toMath(page);
        return QStringLiteral("(%1, %2)").arg(geom::formatNumber(m.x(), 2), geom::formatNumber(m.y(), 2));
    };
    switch (kind) {
    case ConstructKind::Point:
        return pts.isEmpty() ? QString() : prefix + coords(pts[0]);
    case ConstructKind::Segment: {
        if (pts.size() < 2)
            return QString();
        return prefix + cs.formatLength(cs.mathDistance(pts[0], pts[1]), 2);
    }
    case ConstructKind::Line:
    case ConstructKind::Ray: {
        if (pts.size() < 2)
            return QString();
        double m = 0.0;
        if (!cs.mathSlope(pts[0], pts[1], &m)) {
            return prefix + QStringLiteral("x = ") + geom::formatNumber(cs.toMath(pts[0]).x(), 2);
        }
        const double b = cs.toMath(pts[0]).y() - m * cs.toMath(pts[0]).x();
        QString eq = QStringLiteral("y = ") + geom::formatNumber(m, 2) + QStringLiteral("x");
        if (std::abs(b) > 1e-9)
            eq += (b > 0 ? QStringLiteral(" + ") : QStringLiteral(" − ")) + geom::formatNumber(std::abs(b), 2);
        return prefix + eq;
    }
    case ConstructKind::Vector: {
        if (pts.size() < 2)
            return QString();
        const QPointF d = cs.toMath(pts[1]) - cs.toMath(pts[0]);
        if (cs.usesScale()) {
            return prefix + QStringLiteral("⟨%1, %2⟩ %3  |v| = %4")
                                .arg(geom::formatNumber(cs.toReal(d.x()), 2), geom::formatNumber(cs.toReal(d.y()), 2),
                                     cs.realUnitLabel(), cs.formatLength(geom::length(d), 2));
        }
        return prefix + QStringLiteral("⟨%1, %2⟩  |v| = %3")
                            .arg(geom::formatNumber(d.x(), 2), geom::formatNumber(d.y(), 2),
                                 geom::formatNumber(geom::length(d), 2));
    }
    }
    return QString();
}

QString GeometryObject::labelText(const CoordinateSystem& cs) const
{
    return format(m_kind, pagePoints(), m_name, cs);
}

void GeometryObject::paintShape(QPainter& p, ConstructKind kind, const QVector<QPointF>& pts, const QColor& color,
                                const QString& label)
{
    if (pts.isEmpty())
        return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPen pen(color, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    auto dot = [&](const QPointF& c, qreal r) {
        p.setPen(QPen(QColor(15, 15, 15), 1.5));
        p.setBrush(color);
        p.drawEllipse(c, r, r);
    };
    QPointF labelAnchor = pts[0] + QPointF(0, -34);
    switch (kind) {
    case ConstructKind::Point:
        dot(pts[0], 8);
        break;
    case ConstructKind::Segment:
    case ConstructKind::Line:
    case ConstructKind::Ray: {
        if (pts.size() < 2)
            break;
        QPointF a, b;
        drawnSegment(kind, pts, a, b);
        p.setPen(pen);
        p.drawLine(a, b);
        dot(pts[0], 6.5);
        dot(pts[1], 6.5);
        labelAnchor = labelBesideSegment(pts[0], pts[1], label, 12.0, 20.0);
        break;
    }
    case ConstructKind::Vector: {
        if (pts.size() < 2)
            break;
        ShapeObject::paintArrow(p, pts[0], pts[1], pen, false, true);
        dot(pts[0], 5.5);
        labelAnchor = labelBesideSegment(pts[0], pts[1], label, 14.0, 20.0);
        break;
    }
    }
    p.restore();
    paintValueLabel(p, labelAnchor, label, color, 0.0, 20.0);
}

void GeometryObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    static const CoordinateSystem fallback;
    QString label;
    if (m_showLabel) {
        const QVector<QPointF> pagePts = pagePoints();
        const CoordinateSystem* cs = resolveCoordinateSystem(ctx.page, ctx.coordinates, pagePts, this);
        label = format(m_kind, pagePts, m_name, cs ? *cs : fallback);
    } else if (!m_name.isEmpty()) {
        label = m_name;
    }
    paintShape(painter, m_kind, m_points, m_color, label);
}

void GeometryObject::paintPreview(QPainter& painter, ConstructKind kind, const QVector<QPointF>& pagePoints,
                                  const QColor& color, const CoordinateSystem& cs, bool showLabel)
{
    paintShape(painter, kind, pagePoints, color, showLabel ? format(kind, pagePoints, QString(), cs) : QString());
}

bool GeometryObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    const qreal reach = tolerance + 7.0;
    for (const QPointF& p : m_points)
        if (geom::distance(local, p) <= reach + 4)
            return true;
    if (m_kind == ConstructKind::Point || m_points.size() < 2)
        return false;
    QPointF a, b;
    drawnSegment(m_kind, m_points, a, b);
    return geom::distanceToSegment(local, a, b) <= reach;
}

void GeometryObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(newSize);
    Q_UNUSED(hint);
}

void GeometryObject::setControlPoint(int index, const QPointF& local)
{
    if (index < 0 || index >= m_points.size())
        return;
    m_points[index] = local;
    recenter();
}

void GeometryObject::translateContent(const QPointF& delta)
{
    for (QPointF& p : m_points)
        p += delta;
}

bool GeometryObject::setColor(const QColor& color)
{
    m_color = color;
    return true;
}

std::unique_ptr<DocumentObject> GeometryObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new GeometryObject(*this));
}

void GeometryObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("kind"), constructKindName(m_kind));
    obj.insert(QStringLiteral("pts"), json::fromPoints(m_points));
    obj.insert(QStringLiteral("color"), json::fromColor(m_color));
    obj.insert(QStringLiteral("label"), m_showLabel);
    if (!m_name.isEmpty())
        obj.insert(QStringLiteral("name"), m_name);
}

bool GeometryObject::readProperties(const QJsonObject& obj)
{
    m_kind = constructKindFromName(obj.value(QStringLiteral("kind")).toString());
    m_points = json::toPoints(obj.value(QStringLiteral("pts")));
    m_color = json::toColor(obj.value(QStringLiteral("color")), m_color);
    m_showLabel = obj.value(QStringLiteral("label")).toBool(true);
    m_name = obj.value(QStringLiteral("name")).toString();
    const int needed = m_kind == ConstructKind::Point ? 1 : 2;
    return m_points.size() >= needed;
}

} // namespace cb
