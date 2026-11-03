#include "ai/ShapeRecognizer.h"

#include "core/Geometry.h"
#include "document/ShapeObject.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cb {

namespace {

double maxDeviationFromPolygon(const QVector<QPointF>& points, const QVector<QPointF>& poly, bool closed)
{
    double worst = 0.0;
    const int m = poly.size();
    for (const QPointF& p : points) {
        double best = std::numeric_limits<double>::max();
        const int edges = closed ? m : m - 1;
        for (int i = 0; i < edges; ++i)
            best = std::min(best, geom::distanceToSegment(p, poly[i], poly[(i + 1) % m]));
        worst = std::max(worst, best);
    }
    return worst;
}

/// Interior angle at vertex i of a closed polygon, in degrees.
double cornerAngle(const QVector<QPointF>& poly, int i)
{
    const int m = poly.size();
    const QPointF a = poly[(i + m - 1) % m] - poly[i];
    const QPointF b = poly[(i + 1) % m] - poly[i];
    const double la = geom::length(a);
    const double lb = geom::length(b);
    if (la < 1e-9 || lb < 1e-9)
        return 180.0;
    return geom::radToDeg(std::acos(std::clamp(geom::dot(a, b) / (la * lb), -1.0, 1.0)));
}

QVector<QPointF> closedVertices(const QVector<QPointF>& points, double eps)
{
    QVector<QPointF> pts = points;
    pts.push_back(points.first());
    const QVector<int> idx = geom::simplifyIndices(pts, eps);
    QVector<QPointF> v;
    for (int i : idx)
        v.push_back(pts[i]);
    if (v.size() > 1 && geom::distance(v.first(), v.last()) < eps)
        v.removeLast();
    // Remove nearly straight vertices (e.g. the stroke started mid-edge).
    bool changed = true;
    while (changed && v.size() > 3) {
        changed = false;
        for (int i = 0; i < v.size(); ++i) {
            if (cornerAngle(v, i) > 160.0) {
                v.remove(i);
                changed = true;
                break;
            }
        }
    }
    return v;
}

} // namespace

ShapeRecognizer::Result ShapeRecognizer::classify(const QVector<QPointF>& points, qreal pixel) const
{
    Result r;
    if (points.size() < 4)
        return r;
    const QRectF box = geom::boundingRect(points);
    const double diag = std::hypot(box.width(), box.height());
    if (diag < 30.0 * pixel)
        return r;
    const double length = geom::polylineLength(points);
    const double gap = geom::distance(points.first(), points.last());
    const bool closed = gap < std::max(0.2 * diag, 14.0 * pixel) && length > 1.8 * diag;

    if (!closed) {
        if (gap / length > 0.94) {
            const double dev = maxDeviationFromPolygon(points, {points.first(), points.last()}, false);
            if (dev < 0.06 * gap) {
                QPointF a = points.first();
                QPointF b = points.last();
                // Snap nearly horizontal / vertical lines.
                const double angle = geom::angleDeg(b - a);
                bool snapped = false;
                const double s = geom::snapAngle(angle, 90.0, 4.0, &snapped);
                if (snapped)
                    b = a + geom::rotated(QPointF(gap, 0), s);
                r.kind = Result::Kind::Line;
                r.points = {a, b};
                r.confidence = 1.0 - dev / (0.06 * gap);
            }
        }
        return r;
    }

    const QVector<QPointF> poly = closedVertices(points, 0.07 * diag);
    const double polyErr = poly.size() >= 3 ? maxDeviationFromPolygon(points, poly, true) / diag : 1.0;

    auto polygonResult = [&]() {
        if (poly.size() == 4) {
            bool rightAngles = true;
            for (int i = 0; i < 4; ++i)
                rightAngles = rightAngles && std::abs(cornerAngle(poly, i) - 90.0) < 16.0;
            bool axisAligned = true;
            for (int i = 0; i < 4; ++i) {
                const double a = geom::angleDeg(poly[(i + 1) % 4] - poly[i]);
                bool snapped = false;
                geom::snapAngle(a, 90.0, 10.0, &snapped);
                axisAligned = axisAligned && snapped;
            }
            if (rightAngles && axisAligned) {
                r.kind = Result::Kind::Rectangle;
                r.box = geom::boundingRect(poly);
                r.confidence = 1.0 - polyErr / 0.06;
                return;
            }
        }
        r.kind = Result::Kind::Polygon;
        r.points = poly;
        r.confidence = 1.0 - polyErr / 0.06;
    };

    if (poly.size() >= 3 && poly.size() <= 4 && polyErr < 0.06) {
        polygonResult();
        return r;
    }

    // Ellipse fit on the bounding box.
    const double rx = box.width() / 2;
    const double ry = box.height() / 2;
    if (rx > 1e-6 && ry > 1e-6) {
        const QPointF c = box.center();
        double err = 0.0;
        for (const QPointF& p : points) {
            const double dx = (p.x() - c.x()) / rx;
            const double dy = (p.y() - c.y()) / ry;
            err += std::abs(std::sqrt(dx * dx + dy * dy) - 1.0);
        }
        err /= points.size();
        if (err < 0.09) {
            if (std::abs(rx - ry) / std::max(rx, ry) < 0.16) {
                const double rr = (rx + ry) / 2;
                r.kind = Result::Kind::Circle;
                r.box = QRectF(c.x() - rr, c.y() - rr, 2 * rr, 2 * rr);
            } else {
                r.kind = Result::Kind::Ellipse;
                r.box = box;
            }
            r.confidence = 1.0 - err / 0.09;
            return r;
        }
    }

    if (poly.size() >= 5 && poly.size() <= 6 && polyErr < 0.05)
        polygonResult();
    return r;
}

ObjectPtr ShapeRecognizer::recognize(const QVector<QPointF>& points, const InkStyle& ink, qreal pixel) const
{
    const Result r = classify(points, pixel);
    ShapeStyle style;
    style.stroke = ink.color;
    style.width = ink.width;
    style.dashed = ink.style == StrokeStyle::Dashed;
    switch (r.kind) {
    case Result::Kind::Line:
        return ShapeObject::createLine(ShapeKind::Line, r.points[0], r.points[1], style);
    case Result::Kind::Circle:
        return ShapeObject::createBox(ShapeKind::Circle, r.box, style);
    case Result::Kind::Ellipse:
        return ShapeObject::createBox(ShapeKind::Ellipse, r.box, style);
    case Result::Kind::Rectangle:
        return ShapeObject::createBox(ShapeKind::Rectangle, r.box, style);
    case Result::Kind::Polygon:
        return ShapeObject::createPolygon(r.points, style);
    case Result::Kind::None:
        break;
    }
    return nullptr;
}

} // namespace cb
