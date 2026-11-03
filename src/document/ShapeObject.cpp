#include "document/ShapeObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"
#include "document/StrokeObject.h"

#include <QCoreApplication>
#include <QPainter>
#include <QPainterPathStroker>

#include <algorithm>
#include <cmath>

namespace cb {

namespace {
struct KindInfo
{
    ShapeKind kind;
    const char* name;
    const char* label;
};
const KindInfo kKinds[] = {
    {ShapeKind::Line, "line", QT_TRANSLATE_NOOP("Shapes", "Line")},
    {ShapeKind::Arrow, "arrow", QT_TRANSLATE_NOOP("Shapes", "Arrow")},
    {ShapeKind::DoubleArrow, "double-arrow", QT_TRANSLATE_NOOP("Shapes", "Double arrow")},
    {ShapeKind::Rectangle, "rectangle", QT_TRANSLATE_NOOP("Shapes", "Rectangle")},
    {ShapeKind::RoundedRect, "rounded-rectangle", QT_TRANSLATE_NOOP("Shapes", "Rounded rectangle")},
    {ShapeKind::Ellipse, "ellipse", QT_TRANSLATE_NOOP("Shapes", "Ellipse")},
    {ShapeKind::Circle, "circle", QT_TRANSLATE_NOOP("Shapes", "Circle")},
    {ShapeKind::Triangle, "triangle", QT_TRANSLATE_NOOP("Shapes", "Triangle")},
    {ShapeKind::RightTriangle, "right-triangle", QT_TRANSLATE_NOOP("Shapes", "Right triangle")},
    {ShapeKind::Diamond, "diamond", QT_TRANSLATE_NOOP("Shapes", "Diamond")},
    {ShapeKind::Parallelogram, "parallelogram", QT_TRANSLATE_NOOP("Shapes", "Parallelogram")},
    {ShapeKind::Hexagon, "hexagon", QT_TRANSLATE_NOOP("Shapes", "Hexagon")},
    {ShapeKind::RegularPolygon, "polygon", QT_TRANSLATE_NOOP("Shapes", "Polygon")},
    {ShapeKind::FreePolygon, "free-polygon", QT_TRANSLATE_NOOP("Shapes", "Free polygon")},
};
} // namespace

QString shapeKindName(ShapeKind kind)
{
    for (const auto& k : kKinds)
        if (k.kind == kind)
            return QString::fromLatin1(k.name);
    return QStringLiteral("rectangle");
}

ShapeKind shapeKindFromName(const QString& name)
{
    for (const auto& k : kKinds)
        if (name == QLatin1String(k.name))
            return k.kind;
    return ShapeKind::Rectangle;
}

QString shapeKindLabel(ShapeKind kind)
{
    for (const auto& k : kKinds)
        if (k.kind == kind)
            return QCoreApplication::translate("Shapes", k.label);
    return QString();
}

bool isLineShape(ShapeKind kind)
{
    return kind == ShapeKind::Line || kind == ShapeKind::Arrow || kind == ShapeKind::DoubleArrow;
}

ShapeObject::ShapeObject()
    : DocumentObject(ObjectType::Shape)
{
}

std::unique_ptr<ShapeObject> ShapeObject::createBox(ShapeKind kind, const QRectF& pageRect, const ShapeStyle& style,
                                                    int sides)
{
    auto s = std::make_unique<ShapeObject>();
    s->m_kind = kind;
    s->m_style = style;
    s->m_sides = std::clamp(sides, 3, 24);
    const QRectF r = pageRect.normalized();
    s->m_size = QSizeF(std::max(2.0, r.width()), std::max(2.0, r.height()));
    s->setPosition(r.center());
    return s;
}

std::unique_ptr<ShapeObject> ShapeObject::createLine(ShapeKind kind, const QPointF& a, const QPointF& b,
                                                     const ShapeStyle& style)
{
    auto s = std::make_unique<ShapeObject>();
    s->m_kind = kind;
    s->m_style = style;
    const QPointF c = geom::midpoint(a, b);
    s->setPosition(c);
    s->m_p1 = a - c;
    s->m_p2 = b - c;
    return s;
}

std::unique_ptr<ShapeObject> ShapeObject::createPolygon(const QVector<QPointF>& pagePoints, const ShapeStyle& style)
{
    auto s = std::make_unique<ShapeObject>();
    s->m_kind = ShapeKind::FreePolygon;
    s->m_style = style;
    const QPointF c = geom::boundingRect(pagePoints).center();
    s->setPosition(c);
    for (const QPointF& p : pagePoints)
        s->m_points.push_back(p - c);
    return s;
}

QPainterPath ShapeObject::outlineFor(ShapeKind kind, const QSizeF& size, int sides)
{
    const qreal w = size.width();
    const qreal h = size.height();
    const QRectF r(-w / 2, -h / 2, w, h);
    QPainterPath path;
    auto polygon = [&](const QVector<QPointF>& pts) {
        path.moveTo(pts.first());
        for (int i = 1; i < pts.size(); ++i)
            path.lineTo(pts[i]);
        path.closeSubpath();
    };
    switch (kind) {
    case ShapeKind::Rectangle:
        path.addRect(r);
        break;
    case ShapeKind::RoundedRect: {
        const qreal radius = std::min(w, h) * 0.18;
        path.addRoundedRect(r, radius, radius);
        break;
    }
    case ShapeKind::Ellipse:
    case ShapeKind::Circle:
        path.addEllipse(r);
        break;
    case ShapeKind::Triangle:
        polygon({QPointF(0, r.top()), r.bottomRight(), r.bottomLeft()});
        break;
    case ShapeKind::RightTriangle:
        polygon({r.topLeft(), r.bottomRight(), r.bottomLeft()});
        break;
    case ShapeKind::Diamond:
        polygon({QPointF(0, r.top()), QPointF(r.right(), 0), QPointF(0, r.bottom()), QPointF(r.left(), 0)});
        break;
    case ShapeKind::Parallelogram: {
        const qreal skew = w * 0.22;
        polygon({QPointF(r.left() + skew, r.top()), r.topRight(), QPointF(r.right() - skew, r.bottom()), r.bottomLeft()});
        break;
    }
    case ShapeKind::Hexagon:
    case ShapeKind::RegularPolygon: {
        const int n = kind == ShapeKind::Hexagon ? 6 : std::clamp(sides, 3, 24);
        QVector<QPointF> pts;
        // Start at the top for odd counts, flat top for hexagons.
        const double start = kind == ShapeKind::Hexagon ? 0.0 : -90.0;
        for (int i = 0; i < n; ++i) {
            const double a = geom::degToRad(start + 360.0 * i / n);
            pts.push_back(QPointF(std::cos(a) * w / 2, std::sin(a) * h / 2));
        }
        polygon(pts);
        break;
    }
    case ShapeKind::Line:
    case ShapeKind::Arrow:
    case ShapeKind::DoubleArrow:
        path.moveTo(-w / 2, 0);
        path.lineTo(w / 2, 0);
        break;
    case ShapeKind::FreePolygon:
        polygon({QPointF(r.left(), r.top() + h * 0.3), QPointF(r.left() + w * 0.6, r.top()), r.bottomRight(),
                 QPointF(r.left() + w * 0.2, r.bottom())});
        break;
    }
    return path;
}

QPainterPath ShapeObject::outline() const
{
    if (m_kind == ShapeKind::FreePolygon) {
        QPainterPath path;
        if (m_points.isEmpty())
            return path;
        path.moveTo(m_points.first());
        for (int i = 1; i < m_points.size(); ++i)
            path.lineTo(m_points[i]);
        path.closeSubpath();
        return path;
    }
    if (isLineShape(m_kind)) {
        QPainterPath path(m_p1);
        path.lineTo(m_p2);
        return path;
    }
    return outlineFor(m_kind, m_size, m_sides);
}

QRectF ShapeObject::localBounds() const
{
    if (isLineShape(m_kind))
        return QRectF(m_p1, m_p2).normalized();
    if (m_kind == ShapeKind::FreePolygon)
        return geom::boundingRect(m_points);
    return QRectF(-m_size.width() / 2, -m_size.height() / 2, m_size.width(), m_size.height());
}

qreal ShapeObject::outlineMargin() const
{
    const qreal head = isLineShape(m_kind) && m_kind != ShapeKind::Line ? std::max(14.0, m_style.width * 3.5) : 0.0;
    return m_style.width / 2 + head + 2.0;
}

void ShapeObject::arrowGeometry(const QPointF& a, const QPointF& b, qreal penWidth, bool startHead, bool endHead,
                                QLineF* shaft, QVector<QPolygonF>* heads)
{
    const qreal len = geom::distance(a, b);
    const qreal head = std::min(len * 0.3, std::max(8.0, penWidth * 3.5));
    const QPointF dir = len > 1e-6 ? (b - a) / len : QPointF(1, 0);
    const QPointF n = geom::perpendicular(dir);
    QPointF from = a;
    QPointF to = b;
    if (startHead)
        from = a + dir * head * 0.8;
    if (endHead)
        to = b - dir * head * 0.8;
    if (shaft)
        *shaft = QLineF(from, to);
    if (!heads)
        return;
    auto makeHead = [&](const QPointF& tip, const QPointF& d) {
        QPolygonF tri;
        tri << tip << tip - d * head + n * head * 0.5 << tip - d * head - n * head * 0.5;
        heads->push_back(tri);
    };
    if (endHead)
        makeHead(b, dir);
    if (startHead)
        makeHead(a, -dir);
}

void ShapeObject::paintArrow(QPainter& painter, const QPointF& a, const QPointF& b, const QPen& pen, bool startHead,
                             bool endHead)
{
    QLineF shaft;
    QVector<QPolygonF> heads;
    arrowGeometry(a, b, pen.widthF(), startHead, endHead, &shaft, &heads);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(shaft);
    QPen headPen = pen;
    headPen.setStyle(Qt::SolidLine);
    headPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(headPen);
    painter.setBrush(pen.color());
    for (const QPolygonF& tri : heads)
        painter.drawPolygon(tri);
}

namespace {
// Straight pieces become strokes with points every couple of units: ink is drawn as a smooth
// curve through its points, so dense points keep sides straight and corners sharp.
QVector<StrokePoint> densified(const QPolygonF& polyline, qreal step)
{
    QVector<StrokePoint> out;
    for (int i = 0; i < polyline.size(); ++i) {
        if (i > 0) {
            const QPointF a = polyline[i - 1];
            const QPointF b = polyline[i];
            const int n = std::max(1, static_cast<int>(std::ceil(geom::distance(a, b) / step)));
            for (int k = 1; k < n; ++k)
                out.push_back({geom::lerp(a, b, double(k) / n), 1.0f});
        }
        out.push_back({polyline[i], 1.0f});
    }
    return out;
}
} // namespace

std::vector<std::unique_ptr<DocumentObject>> ShapeObject::toInk() const
{
    std::vector<std::unique_ptr<DocumentObject>> out;
    InkStyle ink;
    ink.color = m_style.stroke;
    ink.width = m_style.width;
    ink.style = m_style.dashed ? StrokeStyle::Dashed : StrokeStyle::Pen;
    ink.pressure = false;
    const qreal step = 2.0;
    auto addStroke = [&](const QPolygonF& polyline) {
        if (polyline.size() >= 2)
            out.push_back(StrokeObject::fromPagePoints(densified(polyline, step), ink));
    };
    const QTransform t = transform();
    if (isLineShape(m_kind)) {
        QLineF shaft;
        QVector<QPolygonF> heads;
        arrowGeometry(m_p1, m_p2, m_style.width, m_kind == ShapeKind::DoubleArrow,
                      m_kind == ShapeKind::Arrow || m_kind == ShapeKind::DoubleArrow, &shaft, &heads);
        QPolygonF line;
        line << t.map(shaft.p1()) << t.map(shaft.p2());
        addStroke(line);
        ShapeStyle headStyle = m_style;
        headStyle.dashed = false;
        headStyle.fill = m_style.stroke;
        for (const QPolygonF& head : heads)
            out.push_back(createPolygon(t.map(head), headStyle));
        return out;
    }
    if (m_style.stroke.alpha() == 0 || m_style.width <= 0.0)
        return out; // a fill without an outline has no lines to keep
    for (const QPolygonF& polyline : outline().toSubpathPolygons(t))
        addStroke(polyline);
    return out;
}

void ShapeObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_style.stroke, m_style.width, m_style.dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    if (isLineShape(m_kind)) {
        paintArrow(painter, m_p1, m_p2, pen, m_kind == ShapeKind::DoubleArrow,
                   m_kind == ShapeKind::Arrow || m_kind == ShapeKind::DoubleArrow);
    } else {
        painter.setPen(pen);
        painter.setBrush(m_style.fill.alpha() > 0 ? QBrush(m_style.fill) : Qt::NoBrush);
        painter.drawPath(outline());
    }
    painter.restore();
}

bool ShapeObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    const qreal reach = tolerance + m_style.width / 2;
    if (isLineShape(m_kind))
        return geom::distanceToSegment(local, m_p1, m_p2) <= reach + 2;
    const QPainterPath path = outline();
    if (m_style.fill.alpha() > 0 && path.contains(local))
        return true;
    QPainterPathStroker stroker;
    stroker.setWidth(2 * reach + 2);
    return stroker.createStroke(path).contains(local);
}

bool ShapeObject::enclosesPoint(const QPointF& pagePos) const
{
    return !isLineShape(m_kind) && outline().contains(mapFromPage(pagePos));
}

QVector<QPointF> ShapeObject::controlPoints() const
{
    if (isLineShape(m_kind))
        return {m_p1, m_p2};
    if (m_kind == ShapeKind::FreePolygon)
        return m_points;
    return {};
}

void ShapeObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    if (m_kind == ShapeKind::FreePolygon) {
        const QRectF old = localBounds();
        const double sx = old.width() > 0.5 ? newSize.width() / old.width() : 1.0;
        const double sy = old.height() > 0.5 ? newSize.height() / old.height() : 1.0;
        for (QPointF& p : m_points)
            p = old.center() + QPointF((p.x() - old.center().x()) * sx, (p.y() - old.center().y()) * sy);
        return;
    }
    if (isLineShape(m_kind)) {
        const QRectF old = localBounds();
        const double sx = old.width() > 0.5 ? newSize.width() / old.width() : 1.0;
        const double sy = old.height() > 0.5 ? newSize.height() / old.height() : 1.0;
        m_p1 = QPointF(m_p1.x() * sx, m_p1.y() * sy);
        m_p2 = QPointF(m_p2.x() * sx, m_p2.y() * sy);
        return;
    }
    m_size = newSize;
    if (m_kind == ShapeKind::Circle) {
        const qreal d = std::max(newSize.width(), newSize.height());
        m_size = QSizeF(d, d);
    }
}

void ShapeObject::setControlPoint(int index, const QPointF& local)
{
    if (isLineShape(m_kind)) {
        if (index == 0)
            m_p1 = local;
        else if (index == 1)
            m_p2 = local;
    } else if (m_kind == ShapeKind::FreePolygon && index >= 0 && index < m_points.size()) {
        m_points[index] = local;
    }
    recenter();
}

void ShapeObject::translateContent(const QPointF& delta)
{
    m_p1 += delta;
    m_p2 += delta;
    for (QPointF& p : m_points)
        p += delta;
}

bool ShapeObject::setColor(const QColor& color)
{
    m_style.stroke = color;
    if (m_style.fill.alpha() > 0) {
        QColor fill = color;
        fill.setAlpha(m_style.fill.alpha());
        m_style.fill = fill;
    }
    return true;
}

std::unique_ptr<DocumentObject> ShapeObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new ShapeObject(*this));
}

void ShapeObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("kind"), shapeKindName(m_kind));
    obj.insert(QStringLiteral("stroke"), json::fromColor(m_style.stroke));
    obj.insert(QStringLiteral("width"), m_style.width);
    if (m_style.fill.alpha() > 0)
        obj.insert(QStringLiteral("fill"), json::fromColor(m_style.fill));
    if (m_style.dashed)
        obj.insert(QStringLiteral("dashed"), true);
    if (isLineShape(m_kind)) {
        obj.insert(QStringLiteral("p1"), json::fromPoint(m_p1));
        obj.insert(QStringLiteral("p2"), json::fromPoint(m_p2));
    } else if (m_kind == ShapeKind::FreePolygon) {
        obj.insert(QStringLiteral("pts"), json::fromPoints(m_points));
    } else {
        obj.insert(QStringLiteral("size"), json::fromSize(m_size));
        if (m_kind == ShapeKind::RegularPolygon)
            obj.insert(QStringLiteral("sides"), m_sides);
    }
}

bool ShapeObject::readProperties(const QJsonObject& obj)
{
    m_kind = shapeKindFromName(obj.value(QStringLiteral("kind")).toString());
    m_style.stroke = json::toColor(obj.value(QStringLiteral("stroke")));
    m_style.width = std::max(0.5, obj.value(QStringLiteral("width")).toDouble(4.0));
    m_style.fill = json::toColor(obj.value(QStringLiteral("fill")), Qt::transparent);
    m_style.dashed = obj.value(QStringLiteral("dashed")).toBool(false);
    m_p1 = json::toPoint(obj.value(QStringLiteral("p1")), QPointF(-50, 0));
    m_p2 = json::toPoint(obj.value(QStringLiteral("p2")), QPointF(50, 0));
    m_points = json::toPoints(obj.value(QStringLiteral("pts")));
    m_size = json::toSize(obj.value(QStringLiteral("size")), QSizeF(100, 100));
    m_sides = std::clamp(obj.value(QStringLiteral("sides")).toInt(5), 3, 24);
    if (m_kind == ShapeKind::FreePolygon && m_points.size() < 3)
        return false;
    return true;
}

} // namespace cb
