#include "geometry/Precision.h"

#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/CoordinateResolver.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "geometry/GeometryObject.h"
#include "geometry/MeasurementObject.h"
#include "math/CoordinateSystem.h"

#include <QPolygonF>

#include <cmath>

namespace cb::precision {

namespace {
QPointF fromPolar(double len, double degrees)
{
    const double r = geom::degToRad(degrees);
    return QPointF(len * std::cos(r), len * std::sin(r));
}

bool twoPoints(const QVector<QPointF>& pts)
{
    return pts.size() >= 2;
}
} // namespace

double length(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    return twoPoints(pts) ? cs.mathDistance(pts[0], pts[1]) : 0.0;
}

double direction(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    return twoPoints(pts) ? cs.mathAngle(pts[0], pts[1]) : 0.0;
}

QVector<QPointF> withPolar(const QVector<QPointF>& pts, double mathLength, double degrees, const CoordinateSystem& cs)
{
    if (!twoPoints(pts) || !(mathLength > 0.0) || !std::isfinite(mathLength) || !std::isfinite(degrees))
        return pts;
    QVector<QPointF> out = pts;
    out[1] = cs.toPage(cs.toMath(pts[0]) + fromPolar(mathLength, degrees));
    return out;
}

QVector<QPointF> withLength(const QVector<QPointF>& pts, double mathLength, const CoordinateSystem& cs)
{
    return withPolar(pts, mathLength, direction(pts, cs), cs);
}

QVector<QPointF> withDirection(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs)
{
    const double len = length(pts, cs);
    return withPolar(pts, len > 1e-9 ? len : 1.0, degrees, cs);
}

double angle(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    return MeasurementObject::angleDegrees(pts, cs);
}

QVector<QPointF> withAngle(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs)
{
    if (pts.size() < 3 || !std::isfinite(degrees))
        return pts;
    const QPointF v = cs.toMath(pts[1]);
    const QPointF a = cs.toMath(pts[0]) - v;
    const QPointF b = cs.toMath(pts[2]) - v;
    const double la = geom::length(a);
    double lb = geom::length(b);
    if (la < 1e-9)
        return pts;
    if (lb < 1e-9)
        lb = la;
    // Keep the side on which the angle opens (counter-clockwise unless it currently opens clockwise).
    const double cross = a.x() * b.y() - a.y() * b.x();
    const double sign = cross < -1e-12 ? -1.0 : 1.0;
    const double dirA = geom::radToDeg(std::atan2(a.y(), a.x()));
    QVector<QPointF> out = pts;
    out[2] = cs.toPage(v + fromPolar(lb, dirA + sign * degrees));
    return out;
}

SlopeValues slopeValues(const QVector<QPointF>& pts, const CoordinateSystem& cs)
{
    SlopeValues s;
    if (!twoPoints(pts))
        return s;
    const QPointF d = cs.toMath(pts[1]) - cs.toMath(pts[0]);
    s.rise = d.y();
    s.run = d.x();
    s.vertical = std::abs(s.run) < 1e-9;
    s.slope = s.vertical ? 0.0 : s.rise / s.run;
    s.angle = s.vertical ? 90.0 : geom::radToDeg(std::atan(s.slope));
    return s;
}

QVector<QPointF> withRiseRun(const QVector<QPointF>& pts, double rise, double run, const CoordinateSystem& cs)
{
    if (!twoPoints(pts) || !std::isfinite(rise) || !std::isfinite(run) || (std::abs(rise) < 1e-12 && std::abs(run) < 1e-12))
        return pts;
    QVector<QPointF> out = pts;
    out[1] = cs.toPage(cs.toMath(pts[0]) + QPointF(run, rise));
    return out;
}

QVector<QPointF> withSlope(const QVector<QPointF>& pts, double slope, const CoordinateSystem& cs)
{
    const SlopeValues s = slopeValues(pts, cs);
    double run = s.run;
    if (std::abs(run) < 1e-9)
        run = std::max(1.0, std::abs(s.rise));
    return withRiseRun(pts, slope * run, run, cs);
}

QVector<QPointF> withInclination(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs)
{
    if (!twoPoints(pts) || !std::isfinite(degrees))
        return pts;
    degrees = std::clamp(degrees, -90.0, 90.0);
    const SlopeValues s = slopeValues(pts, cs);
    double len = length(pts, cs);
    if (len < 1e-9)
        len = 1.0;
    const double leftward = s.run < -1e-9 ? -1.0 : 1.0;
    const double r = geom::degToRad(degrees);
    return withRiseRun(pts, len * std::sin(r) * leftward, len * std::cos(r) * leftward, cs);
}

bool shapeMetrics(const DocumentObject& object, const CoordinateSystem& cs, ShapeMetrics* out)
{
    if (!out)
        return false;
    const double ppu = cs.pxPerUnit();
    if (ppu <= 0.0)
        return false;
    if (object.type() == ObjectType::Measurement) {
        const auto& m = static_cast<const MeasurementObject&>(object);
        if (m.kind() != MeasureKind::Area)
            return false;
        const QVector<QPointF> pts = m.pagePoints();
        const QRectF bounds = geom::boundingRect(pts);
        out->width = bounds.width() / ppu;
        out->height = bounds.height() / ppu;
        out->area = cs.mathArea(pts);
        out->perimeter = MeasurementObject::perimeter(pts, cs);
        return true;
    }
    if (object.type() != ObjectType::Shape)
        return false;
    const auto& shape = static_cast<const ShapeObject&>(object);
    if (isLineShape(shape.kind()))
        return false;
    QVector<QPointF> polygon;
    if (shape.kind() == ShapeKind::FreePolygon) {
        for (const QPointF& p : shape.controlPoints())
            polygon.push_back(shape.mapToPage(p));
        const QRectF b = geom::boundingRect(polygon);
        out->width = b.width() / ppu;
        out->height = b.height() / ppu;
    } else {
        const QSizeF size = shape.boxSize();
        out->width = size.width() / ppu;
        out->height = size.height() / ppu;
        if (shape.kind() == ShapeKind::Ellipse || shape.kind() == ShapeKind::Circle) {
            const double a = out->width / 2.0;
            const double b = out->height / 2.0;
            out->area = geom::kPi * a * b;
            // Ramanujan's approximation (exact for circles).
            const double h = ((a - b) * (a - b)) / ((a + b) * (a + b));
            out->perimeter = geom::kPi * (a + b) * (1.0 + 3.0 * h / (10.0 + std::sqrt(4.0 - 3.0 * h)));
            return true;
        }
        const QPolygonF poly = ShapeObject::outlineFor(shape.kind(), size, shape.sides()).toFillPolygon();
        for (const QPointF& p : poly)
            polygon.push_back(shape.mapToPage(p));
        if (polygon.size() > 1 && polygon.first() == polygon.last())
            polygon.removeLast();
    }
    if (polygon.size() < 3)
        return false;
    out->area = cs.mathArea(polygon);
    double perimeter = 0.0;
    for (int i = 0; i < polygon.size(); ++i)
        perimeter += cs.mathDistance(polygon[i], polygon[(i + 1) % polygon.size()]);
    out->perimeter = perimeter;
    return true;
}

QVector<QPointF> definingPoints(const DocumentObject& object)
{
    switch (object.type()) {
    case ObjectType::Measurement:
        return static_cast<const MeasurementObject&>(object).pagePoints();
    case ObjectType::Geometry:
        return static_cast<const GeometryObject&>(object).pagePoints();
    case ObjectType::Shape: {
        const auto& shape = static_cast<const ShapeObject&>(object);
        if (!isLineShape(shape.kind()))
            return {};
        QVector<QPointF> pts;
        for (const QPointF& p : shape.controlPoints())
            pts.push_back(shape.mapToPage(p));
        return pts;
    }
    default:
        return {};
    }
}

bool applyPoints(Document& doc, Page& page, const ObjectId& id, const QVector<QPointF>& newPagePoints,
                 const QString& text)
{
    DocumentObject* object = page.object(id);
    if (!object)
        return false;
    const QVector<QPointF> current = definingPoints(*object);
    if (current.size() != newPagePoints.size() || current.isEmpty())
        return false;
    for (const QPointF& p : newPagePoints)
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()))
            return false;
    ObjectPtr before = object->clone();
    ObjectPtr after = object->clone();
    bool changed = false;
    for (int i = 0; i < newPagePoints.size(); ++i) {
        if (geom::distance(current[i], newPagePoints[i]) < 1e-9)
            continue;
        after->moveControlPointTo(i, newPagePoints[i]);
        changed = true;
    }
    if (!changed)
        return false;
    auto cmd = std::make_unique<ModifyObjectsCommand>(page.id(), text);
    cmd->add(std::move(before), std::move(after));
    doc.commands().push(std::move(cmd));
    return true;
}

bool applyShapeSize(Document& doc, Page& page, const ObjectId& id, double mathWidth, double mathHeight,
                    const CoordinateSystem& cs, const QString& text)
{
    DocumentObject* object = page.object(id);
    if (!object || object->type() != ObjectType::Shape || !object->canResize())
        return false;
    const double ppu = cs.pxPerUnit();
    if (!(mathWidth > 0.0) || !(mathHeight > 0.0) || ppu <= 0.0 || !std::isfinite(mathWidth) || !std::isfinite(mathHeight))
        return false;
    const auto* shape = static_cast<const ShapeObject*>(object);
    const QRectF local = shape->localBounds();
    const QSizeF size(mathWidth * ppu, mathHeight * ppu);
    const QRectF target(local.center() - QPointF(size.width() / 2, size.height() / 2), size);
    if (std::abs(target.width() - local.width()) < 1e-9 && std::abs(target.height() - local.height()) < 1e-9)
        return false;
    ObjectPtr before = object->clone();
    ObjectPtr after = object->clone();
    after->resizeTo(target);
    auto cmd = std::make_unique<ModifyObjectsCommand>(page.id(), text);
    cmd->add(std::move(before), std::move(after));
    doc.commands().push(std::move(cmd));
    return true;
}

CoordinateSystem coordinateSystemFor(const Document& doc, const Page* page, const DocumentObject& object)
{
    const CoordinateSystem global = doc.coordinatesFor(page);
    QVector<QPointF> pts = definingPoints(object);
    if (pts.isEmpty())
        pts.push_back(object.sceneBounds().center());
    const CoordinateSystem* cs = resolveCoordinateSystem(page, &global, pts, &object);
    return cs ? *cs : global;
}

} // namespace cb::precision
