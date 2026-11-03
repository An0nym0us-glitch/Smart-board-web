#include "document/StrokeObject.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"

#include <QJsonArray>
#include <QPainter>
#include <QPolygonF>

#include <algorithm>
#include <cmath>

namespace cb {

QString strokeStyleName(StrokeStyle style)
{
    switch (style) {
    case StrokeStyle::Pen: return QStringLiteral("pen");
    case StrokeStyle::Highlighter: return QStringLiteral("highlighter");
    case StrokeStyle::Dashed: return QStringLiteral("dashed");
    case StrokeStyle::Dotted: return QStringLiteral("dotted");
    }
    return QStringLiteral("pen");
}

StrokeStyle strokeStyleFromName(const QString& name)
{
    if (name == QLatin1String("highlighter"))
        return StrokeStyle::Highlighter;
    if (name == QLatin1String("dashed"))
        return StrokeStyle::Dashed;
    if (name == QLatin1String("dotted"))
        return StrokeStyle::Dotted;
    return StrokeStyle::Pen;
}

StrokeObject::StrokeObject()
    : DocumentObject(ObjectType::Stroke)
{
}

std::unique_ptr<StrokeObject> StrokeObject::fromPagePoints(const QVector<StrokePoint>& pagePoints, const InkStyle& ink)
{
    auto stroke = std::make_unique<StrokeObject>();
    stroke->m_ink = ink;
    if (pagePoints.isEmpty())
        return stroke;
    QVector<QPointF> pts;
    pts.reserve(pagePoints.size());
    for (const StrokePoint& p : pagePoints)
        pts.push_back(p.pos);
    const QPointF center = geom::boundingRect(pts).center();
    stroke->setPosition(center);
    stroke->m_points = pagePoints;
    for (StrokePoint& p : stroke->m_points)
        p.pos -= center;
    stroke->invalidateCache();
    return stroke;
}

QVector<QPointF> StrokeObject::pagePoints() const
{
    const QTransform t = transform();
    QVector<QPointF> out;
    out.reserve(m_points.size());
    for (const StrokePoint& p : m_points)
        out.push_back(t.map(p.pos));
    return out;
}

void StrokeObject::setInk(const InkStyle& ink)
{
    m_ink = ink;
    invalidateCache();
}

void StrokeObject::invalidateCache()
{
    m_pathValid = false;
    m_localBoundsValid = false;
    invalidateBounds();
}

QRectF StrokeObject::localBounds() const
{
    if (!m_localBoundsValid) {
        QVector<QPointF> pts;
        pts.reserve(m_points.size());
        for (const StrokePoint& p : m_points)
            pts.push_back(p.pos);
        m_boundsCache = geom::boundingRect(pts);
        m_localBoundsValid = true;
    }
    return m_boundsCache;
}

qreal StrokeObject::outlineMargin() const
{
    return m_ink.width * 0.5 + 1.0;
}

QColor StrokeObject::effectiveColor(const InkStyle& ink)
{
    QColor c = ink.color;
    if (ink.style == StrokeStyle::Highlighter)
        c.setAlphaF(std::min(c.alphaF(), 0.38));
    return c;
}

QPainterPath StrokeObject::centerlinePath(const QVector<StrokePoint>& pts)
{
    QPainterPath path;
    const int n = pts.size();
    if (n == 0)
        return path;
    path.moveTo(pts[0].pos);
    if (n == 1) {
        path.lineTo(pts[0].pos);
        return path;
    }
    if (n == 2) {
        path.lineTo(pts[1].pos);
        return path;
    }
    // Quadratic curves through midpoints give smooth, C1 continuous ink.
    for (int i = 1; i < n - 1; ++i)
        path.quadTo(pts[i].pos, geom::midpoint(pts[i].pos, pts[i + 1].pos));
    path.lineTo(pts[n - 1].pos);
    return path;
}

QPainterPath StrokeObject::pressureOutline(const QVector<StrokePoint>& pts, qreal width)
{
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    const int n = pts.size();
    if (n == 0)
        return path;
    auto halfWidth = [&](int i) {
        const double pr = std::clamp(static_cast<double>(pts[i].pressure), 0.0, 1.0);
        return std::max(0.35, width * 0.5 * (0.18 + 0.82 * pr));
    };
    if (n == 1) {
        const double r = halfWidth(0);
        path.addEllipse(pts[0].pos, r, r);
        return path;
    }
    QVector<QPointF> left(n), right(n);
    for (int i = 0; i < n; ++i) {
        const QPointF prev = pts[std::max(0, i - 1)].pos;
        const QPointF next = pts[std::min(n - 1, i + 1)].pos;
        QPointF dir = geom::normalized(next - prev);
        if (geom::length(dir) < 0.5)
            dir = QPointF(1, 0);
        const QPointF normal = geom::perpendicular(dir) * halfWidth(i);
        left[i] = pts[i].pos + normal;
        right[i] = pts[i].pos - normal;
    }
    auto appendSmooth = [&path](const QVector<QPointF>& side, bool reverse) {
        const int m = side.size();
        auto at = [&](int k) { return reverse ? side[m - 1 - k] : side[k]; };
        for (int k = 1; k < m - 1; ++k)
            path.quadTo(at(k), geom::midpoint(at(k), at(k + 1)));
        path.lineTo(at(m - 1));
    };
    path.moveTo(left[0]);
    appendSmooth(left, false);
    path.lineTo(right[n - 1]);
    appendSmooth(right, true);
    path.closeSubpath();
    // Round caps and joints at sharp corners.
    path.addEllipse(pts[0].pos, halfWidth(0), halfWidth(0));
    path.addEllipse(pts[n - 1].pos, halfWidth(n - 1), halfWidth(n - 1));
    for (int i = 1; i < n - 1; ++i) {
        const QPointF a = pts[i].pos - pts[i - 1].pos;
        const QPointF b = pts[i + 1].pos - pts[i].pos;
        const double la = geom::length(a);
        const double lb = geom::length(b);
        if (la < 1e-6 || lb < 1e-6)
            continue;
        const double cosAngle = geom::dot(a, b) / (la * lb);
        if (cosAngle < 0.35)
            path.addEllipse(pts[i].pos, halfWidth(i), halfWidth(i));
    }
    return path;
}

void StrokeObject::paintPoints(QPainter& painter, const QVector<StrokePoint>& pts, const InkStyle& ink)
{
    if (pts.isEmpty())
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor c = effectiveColor(ink);
    if (ink.pressure && ink.style == StrokeStyle::Pen) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawPath(pressureOutline(pts, ink.width));
    } else if (pts.size() == 1) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
        painter.drawEllipse(pts[0].pos, ink.width * 0.5, ink.width * 0.5);
    } else {
        QPen pen(c, ink.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (ink.style == StrokeStyle::Dashed) {
            pen.setCapStyle(Qt::FlatCap);
            pen.setDashPattern({3.0, 2.0});
        } else if (ink.style == StrokeStyle::Dotted) {
            pen.setDashPattern({0.01, 2.2});
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(centerlinePath(pts));
    }
    painter.restore();
}

void StrokeObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    Q_UNUSED(ctx);
    if (m_points.isEmpty())
        return;
    const bool filled = (m_ink.pressure && m_ink.style == StrokeStyle::Pen) || m_points.size() == 1;
    if (!m_pathValid) {
        if (m_ink.pressure && m_ink.style == StrokeStyle::Pen) {
            m_pathCache = pressureOutline(m_points, m_ink.width);
        } else if (m_points.size() == 1) {
            m_pathCache = QPainterPath();
            m_pathCache.addEllipse(m_points[0].pos, m_ink.width * 0.5, m_ink.width * 0.5);
        } else {
            m_pathCache = centerlinePath(m_points);
        }
        m_pathValid = true;
    }
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor c = effectiveColor(m_ink);
    if (filled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);
    } else {
        QPen pen(c, m_ink.width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        if (m_ink.style == StrokeStyle::Dashed) {
            pen.setCapStyle(Qt::FlatCap);
            pen.setDashPattern({3.0, 2.0});
        } else if (m_ink.style == StrokeStyle::Dotted) {
            pen.setDashPattern({0.01, 2.2});
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
    }
    painter.drawPath(m_pathCache);
    painter.restore();
}

bool StrokeObject::hitTestLocal(const QPointF& local, qreal tolerance) const
{
    const double reach = tolerance + m_ink.width * 0.5;
    const int n = m_points.size();
    if (n == 0)
        return false;
    if (n == 1)
        return geom::distance(local, m_points[0].pos) <= reach;
    for (int i = 1; i < n; ++i) {
        if (geom::distanceToSegment(local, m_points[i - 1].pos, m_points[i].pos) <= reach)
            return true;
    }
    return false;
}

bool StrokeObject::isInsidePolygon(const QPolygonF& pagePolygon) const
{
    if (m_points.isEmpty())
        return false;
    const QTransform t = transform();
    const int n = m_points.size();
    const int step = std::max(1, n / 64);
    int inside = 0;
    int total = 0;
    for (int i = 0; i < n; i += step) {
        ++total;
        if (pagePolygon.containsPoint(t.map(m_points[i].pos), Qt::OddEvenFill))
            ++inside;
    }
    return inside * 2 > total;
}

bool StrokeObject::setColor(const QColor& color)
{
    m_ink.color = color;
    invalidateCache();
    return true;
}

std::unique_ptr<DocumentObject> StrokeObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new StrokeObject(*this));
}

std::vector<std::unique_ptr<StrokeObject>> StrokeObject::eraseCircle(const QPointF& pageCenter, qreal radius,
                                                                     bool* touched) const
{
    std::vector<std::unique_ptr<StrokeObject>> result;
    bool hit = false;
    const QPointF c = mapFromPage(pageCenter);
    const double R = radius + m_ink.width * 0.5;
    const int n = m_points.size();

    auto finish = [&]() {
        if (touched)
            *touched = hit;
    };
    if (n == 0) {
        finish();
        return result;
    }
    if (n == 1) {
        hit = geom::distance(m_points[0].pos, c) <= R;
        finish();
        return result;
    }

    QVector<QVector<StrokePoint>> fragments;
    QVector<StrokePoint> current;
    auto flush = [&]() {
        if (current.size() >= 2) {
            double len = 0.0;
            for (int k = 1; k < current.size(); ++k)
                len += geom::distance(current[k - 1].pos, current[k].pos);
            if (len > 0.75)
                fragments.push_back(current);
        }
        current.clear();
    };
    auto lerpPoint = [](const StrokePoint& a, const StrokePoint& b, double t) {
        StrokePoint p;
        p.pos = geom::lerp(a.pos, b.pos, t);
        p.pressure = static_cast<float>(a.pressure + (b.pressure - a.pressure) * t);
        return p;
    };

    if (geom::distance(m_points[0].pos, c) > R)
        current.push_back(m_points[0]);
    for (int i = 0; i + 1 < n; ++i) {
        const StrokePoint& a = m_points[i];
        const StrokePoint& b = m_points[i + 1];
        double t0 = 0.0;
        double t1 = 0.0;
        if (!geom::segmentCircleInterval(a.pos, b.pos, c, R, t0, t1)) {
            if (current.isEmpty())
                current.push_back(a);
            current.push_back(b);
            continue;
        }
        hit = true;
        if (t0 > 0.0) {
            if (current.isEmpty())
                current.push_back(a);
            current.push_back(lerpPoint(a, b, t0));
        }
        flush();
        if (t1 < 1.0) {
            current.push_back(lerpPoint(a, b, t1));
            current.push_back(b);
        }
    }
    flush();

    if (!hit) {
        finish();
        return result;
    }
    const QTransform t = transform();
    for (QVector<StrokePoint>& frag : fragments) {
        for (StrokePoint& p : frag)
            p.pos = t.map(p.pos);
        result.push_back(fromPagePoints(frag, m_ink));
    }
    finish();
    return result;
}

void StrokeObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    const QRectF old = localBounds();
    const double sx = old.width() > 0.5 ? newSize.width() / old.width() : 1.0;
    const double sy = old.height() > 0.5 ? newSize.height() / old.height() : 1.0;
    const QPointF c = old.center();
    for (StrokePoint& p : m_points) {
        const QPointF d = p.pos - c;
        p.pos = c + QPointF(d.x() * sx, d.y() * sy);
    }
    invalidateCache();
}

void StrokeObject::translateContent(const QPointF& delta)
{
    for (StrokePoint& p : m_points)
        p.pos += delta;
    invalidateCache();
}

namespace {
double round2(double v) { return std::round(v * 100.0) / 100.0; }
double round3(double v) { return std::round(v * 1000.0) / 1000.0; }
} // namespace

void StrokeObject::writeProperties(QJsonObject& obj) const
{
    QJsonArray pts;
    QJsonArray pressures;
    bool anyPressure = false;
    for (const StrokePoint& p : m_points) {
        pts.append(round2(p.pos.x()));
        pts.append(round2(p.pos.y()));
        pressures.append(round3(p.pressure));
        if (p.pressure < 0.999f)
            anyPressure = true;
    }
    obj.insert(QStringLiteral("pts"), pts);
    if (anyPressure)
        obj.insert(QStringLiteral("pr"), pressures);
    obj.insert(QStringLiteral("color"), json::fromColor(m_ink.color));
    obj.insert(QStringLiteral("width"), m_ink.width);
    obj.insert(QStringLiteral("style"), strokeStyleName(m_ink.style));
    if (m_ink.pressure)
        obj.insert(QStringLiteral("pressure"), true);
}

bool StrokeObject::readProperties(const QJsonObject& obj)
{
    const QVector<QPointF> pts = json::toPoints(obj.value(QStringLiteral("pts")));
    const QJsonArray pr = obj.value(QStringLiteral("pr")).toArray();
    m_points.clear();
    m_points.reserve(pts.size());
    for (int i = 0; i < pts.size(); ++i) {
        StrokePoint sp;
        sp.pos = pts[i];
        sp.pressure = i < pr.size() ? static_cast<float>(pr.at(i).toDouble(1.0)) : 1.0f;
        m_points.push_back(sp);
    }
    m_ink.color = json::toColor(obj.value(QStringLiteral("color")));
    m_ink.width = std::max(0.25, obj.value(QStringLiteral("width")).toDouble(4.0));
    m_ink.style = strokeStyleFromName(obj.value(QStringLiteral("style")).toString());
    m_ink.pressure = obj.value(QStringLiteral("pressure")).toBool(false);
    invalidateCache();
    return !m_points.isEmpty();
}

} // namespace cb
