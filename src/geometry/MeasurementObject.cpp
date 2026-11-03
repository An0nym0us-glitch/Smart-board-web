#include "geometry/MeasurementObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"
#include "document/CoordinateResolver.h"
#include "geometry/Label.h"
#include "math/CoordinateSystem.h"

#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace cb {

QString measureKindName(MeasureKind kind)
{
    switch (kind) {
    case MeasureKind::Distance: return QStringLiteral("distance");
    case MeasureKind::Angle: return QStringLiteral("angle");
    case MeasureKind::Slope: return QStringLiteral("slope");
    case MeasureKind::Area: return QStringLiteral("area");
    }
    return QStringLiteral("distance");
}

MeasureKind measureKindFromName(const QString& name)
{
    if (name == QLatin1String("angle"))
        return MeasureKind::Angle;
    if (name == QLatin1String("slope"))
        return MeasureKind::Slope;
    if (name == QLatin1String("area"))
        return MeasureKind::Area;
    return MeasureKind::Distance;
}

MeasurementObject::MeasurementObject()
    : DocumentObject(ObjectType::Measurement)
{
}

std::unique_ptr<MeasurementObject> MeasurementObject::create(MeasureKind kind, const QVector<QPointF>& pagePoints,
                                                             const QColor& color)
{
    auto m = std::make_unique<MeasurementObject>();
    m->m_kind = kind;
    m->m_color = color;
    const QPointF c = geom::boundingRect(pagePoints).center();
    m->setPosition(c);
    for (const QPointF& p : pagePoints)
        m->m_points.push_back(p - c);
    return m;
}

QVector<QPointF> MeasurementObject::pagePoints() const
{
    QVector<QPointF> out;
    const QTransform t = transform();
    for (const QPointF& p : m_points)
        out.push_back(t.map(p));
    return out;
}

QRectF MeasurementObject::localBounds() const
{
    return geom::boundingRect(m_points);
}

QString MeasurementObject::format(MeasureKind kind, const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    switch (kind) {
    case MeasureKind::Distance:
        if (pts.size() < 2)
            return QString();
        return cs.formatLength(cs.mathDistance(pts[0], pts[1]), 2);
    case MeasureKind::Angle: {
        if (pts.size() < 3)
            return QString();
        const double a = angleDegrees(pts, cs);
        if (a < 0)
            return QString();
        return geom::formatNumber(a, 1) + QStringLiteral("°");
    }
    case MeasureKind::Slope: {
        if (pts.size() < 2)
            return QString();
        double m = 0.0;
        if (!cs.mathSlope(pts[0], pts[1], &m))
            return QObject::tr("m undefined");
        return QStringLiteral("m = ") + geom::formatNumber(m, 3);
    }
    case MeasureKind::Area:
        if (pts.size() < 3)
            return QString();
        return QStringLiteral("A = ") + cs.formatArea(cs.mathArea(pts), 2) + QStringLiteral("  ·  P = ")
            + cs.formatLength(perimeter(pts, cs), 2);
    }
    return QString();
}

double MeasurementObject::angleDegrees(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    if (pts.size() < 3)
        return -1.0;
    const QPointF v = cs.toMath(pts[1]);
    const QPointF a = cs.toMath(pts[0]) - v;
    const QPointF b = cs.toMath(pts[2]) - v;
    const double la = geom::length(a);
    const double lb = geom::length(b);
    if (la < 1e-9 || lb < 1e-9)
        return -1.0;
    const double c = std::clamp(geom::dot(a, b) / (la * lb), -1.0, 1.0);
    return geom::radToDeg(std::acos(c));
}

double MeasurementObject::perimeter(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    double sum = 0.0;
    for (int i = 0; i < pts.size(); ++i)
        sum += cs.mathDistance(pts[i], pts[(i + 1) % pts.size()]);
    return pts.size() >= 3 ? sum : 0.0;
}

QString MeasurementObject::valueText(const CoordinateSystem& cs) const
{
    return format(m_kind, pagePoints(), cs);
}

void MeasurementObject::paintShape(QPainter& p, MeasureKind kind, const QVector<QPointF>& pts, const QColor& color,
                                   const QString& label, qreal rotation, qreal zoom)
{
    Q_UNUSED(zoom);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    auto dot = [&](const QPointF& c) {
        p.save();
        p.setPen(QPen(QColor(20, 20, 20), 1.5));
        p.setBrush(color);
        p.drawEllipse(c, 6.5, 6.5);
        p.restore();
    };

    switch (kind) {
    case MeasureKind::Distance: {
        if (pts.size() < 2)
            break;
        const QPointF a = pts[0];
        const QPointF b = pts[1];
        p.drawLine(a, b);
        const QPointF n = geom::perpendicular(geom::normalized(b - a));
        p.drawLine(a - n * 12, a + n * 12);
        p.drawLine(b - n * 12, b + n * 12);
        paintValueLabel(p, labelBesideSegment(a, b, label, 16.0), label, color, rotation);
        break;
    }
    case MeasureKind::Angle: {
        if (pts.size() < 3) {
            if (pts.size() == 2)
                p.drawLine(pts[1], pts[0]);
            break;
        }
        const QPointF v = pts[1];
        p.drawLine(v, pts[0]);
        p.drawLine(v, pts[2]);
        const double la = geom::distance(v, pts[0]);
        const double lb = geom::distance(v, pts[2]);
        const double r = std::clamp(std::min(la, lb) * 0.4, 18.0, 70.0);
        const double a0 = geom::angleDeg(pts[0] - v);
        const double a1 = geom::angleDeg(pts[2] - v);
        const double sweep = geom::angleDifference(a0, a1);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 50));
        if (std::abs(std::abs(sweep) - 90.0) < 0.5) {
            const QPointF u = geom::normalized(pts[0] - v) * (r * 0.6);
            const QPointF w = geom::normalized(pts[2] - v) * (r * 0.6);
            QPolygonF sq;
            sq << v << v + u << v + u + w << v + w;
            p.drawPolygon(sq);
        } else {
            p.drawPie(QRectF(v.x() - r, v.y() - r, 2 * r, 2 * r), static_cast<int>(-a0 * 16),
                      static_cast<int>(-sweep * 16));
        }
        const QPointF bis = geom::rotated(QPointF(1, 0), a0 + sweep / 2);
        paintValueLabel(p, v + bis * (r + 34), label, color, rotation);
        dot(v);
        break;
    }
    case MeasureKind::Slope: {
        if (pts.size() < 2)
            break;
        const QPointF a = pts[0];
        const QPointF b = pts[1];
        p.drawLine(a, b);
        QPen dash(color, 2.0, Qt::DashLine);
        p.setPen(dash);
        const QPointF corner(b.x(), a.y());
        p.drawLine(a, corner);
        p.drawLine(corner, b);
        paintValueLabel(p, geom::midpoint(a, b) + QPointF(0, -34), label, color, rotation);
        dot(a);
        dot(b);
        break;
    }
    case MeasureKind::Area: {
        if (pts.size() < 2)
            break;
        QPolygonF poly(pts);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 45));
        if (pts.size() >= 3)
            p.drawPolygon(poly);
        else
            p.drawPolyline(poly);
        for (const QPointF& c : pts)
            dot(c);
        QPointF centroid;
        for (const QPointF& c : pts)
            centroid += c;
        centroid /= pts.size();
        paintValueLabel(p, centroid, label, color, rotation);
        break;
    }
    }
    p.restore();
}

void MeasurementObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    static const CoordinateSystem fallback;
    const QVector<QPointF> pagePts = pagePoints();
    const CoordinateSystem* cs = resolveCoordinateSystem(ctx.page, ctx.coordinates, pagePts, this);
    paintShape(painter, m_kind, m_points, m_color, format(m_kind, pagePts, cs ? *cs : fallback), rotation(), ctx.zoom);
}

void MeasurementObject::paintPreview(QPainter& painter, MeasureKind kind, const QVector<QPointF>& pagePoints,
                                     const QColor& color, const CoordinateSystem& cs, qreal zoom)
{
    paintShape(painter, kind, pagePoints, color, format(kind, pagePoints, cs), 0.0, zoom);
}

bool MeasurementObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    const qreal reach = tolerance + 8.0;
    const int n = m_points.size();
    for (int i = 0; i < n; ++i) {
        if (geom::distance(local, m_points[i]) <= reach + 4)
            return true;
        if (i + 1 < n && geom::distanceToSegment(local, m_points[i], m_points[i + 1]) <= reach)
            return true;
    }
    if (m_kind == MeasureKind::Area && n >= 3) {
        if (QPolygonF(m_points).containsPoint(local, Qt::OddEvenFill))
            return true;
        if (geom::distanceToSegment(local, m_points.last(), m_points.first()) <= reach)
            return true;
    }
    if (m_kind == MeasureKind::Slope && n >= 2) {
        const QPointF corner(m_points[1].x(), m_points[0].y());
        if (geom::distanceToSegment(local, m_points[0], corner) <= reach
            || geom::distanceToSegment(local, corner, m_points[1]) <= reach)
            return true;
    }
    return false;
}

void MeasurementObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    const QRectF old = localBounds();
    const double sx = old.width() > 0.5 ? newSize.width() / old.width() : 1.0;
    const double sy = old.height() > 0.5 ? newSize.height() / old.height() : 1.0;
    for (QPointF& p : m_points)
        p = old.center() + QPointF((p.x() - old.center().x()) * sx, (p.y() - old.center().y()) * sy);
}

void MeasurementObject::setControlPoint(int index, const QPointF& local)
{
    if (index < 0 || index >= m_points.size())
        return;
    m_points[index] = local;
    recenter();
}

void MeasurementObject::translateContent(const QPointF& delta)
{
    for (QPointF& p : m_points)
        p += delta;
}

bool MeasurementObject::setColor(const QColor& color)
{
    m_color = color;
    return true;
}

std::unique_ptr<DocumentObject> MeasurementObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new MeasurementObject(*this));
}

void MeasurementObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("kind"), measureKindName(m_kind));
    obj.insert(QStringLiteral("pts"), json::fromPoints(m_points));
    obj.insert(QStringLiteral("color"), json::fromColor(m_color));
}

bool MeasurementObject::readProperties(const QJsonObject& obj)
{
    m_kind = measureKindFromName(obj.value(QStringLiteral("kind")).toString());
    m_points = json::toPoints(obj.value(QStringLiteral("pts")));
    m_color = json::toColor(obj.value(QStringLiteral("color")), m_color);
    return m_points.size() >= 2;
}

} // namespace cb
