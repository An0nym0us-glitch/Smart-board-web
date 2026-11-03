#include "tools/ShapeTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "tools/ToolSettings.h"
#include "ui/Theme.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QPainter>

#include <cmath>

namespace cb {

ShapeTool::ShapeTool(ToolHost& host)
    : Tool(host)
{
}

QRectF ShapeTool::dragRect(Qt::KeyboardModifiers mods) const
{
    QPointF end = m_current;
    const bool square = (mods & Qt::ShiftModifier) || host().settings().shapeKind() == ShapeKind::Circle;
    if (square) {
        const QPointF d = end - m_start;
        const qreal s = std::max(std::abs(d.x()), std::abs(d.y()));
        end = m_start + QPointF(d.x() < 0 ? -s : s, d.y() < 0 ? -s : s);
    }
    return QRectF(m_start, end).normalized();
}

QPointF ShapeTool::lineEnd(Qt::KeyboardModifiers mods) const
{
    if (!(mods & Qt::ShiftModifier))
        return m_current;
    const QPointF d = m_current - m_start;
    const double angle = std::round(geom::angleDeg(d) / 45.0) * 45.0;
    return m_start + geom::rotated(QPointF(geom::length(d), 0), angle);
}

void ShapeTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1 || !host().page())
        return;
    m_pointer = e.pointerId;
    m_start = e.pagePos;
    m_current = e.pagePos;
    m_mods = e.modifiers;
}

void ShapeTool::pointerMove(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    const QRectF before = geom::inflated(QRectF(m_start, m_current).normalized(), 60);
    m_current = e.pagePos;
    m_mods = e.modifiers;
    host().updateOverlay(before.united(geom::inflated(QRectF(m_start, m_current).normalized(), 60)));
}

void ShapeTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    m_current = e.pagePos;
    Page* page = host().page();
    if (!page)
        return;
    const ToolSettings& s = host().settings();
    const ShapeKind kind = s.shapeKind();
    const ShapeStyle style = s.shapeStyle();
    const qreal tapDistance = host().viewToPageLength(host().theme().dp(10));
    const bool tap = geom::distance(m_start, m_current) < tapDistance;

    if (kind == ShapeKind::FreePolygon) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const bool doubleTap = (now - m_lastTapMs) < 400;
        m_lastTapMs = now;
        const QPointF p = m_current;
        if (m_polygon.size() >= 3
            && (geom::distance(p, m_polygon.first()) < host().viewToPageLength(host().theme().dp(22)) || doubleTap)) {
            finishPolygon();
        } else {
            m_polygon.push_back(p);
        }
        host().updateOverlayAll();
        return;
    }

    ObjectPtr shape;
    if (isLineShape(kind)) {
        QPointF a = m_start;
        QPointF b = lineEnd(e.modifiers);
        if (tap) {
            a = m_start - QPointF(90, 0);
            b = m_start + QPointF(90, 0);
        }
        shape = ShapeObject::createLine(kind, a, b, style);
    } else {
        QRectF r = dragRect(e.modifiers);
        if (tap) {
            const QSizeF size = (kind == ShapeKind::Circle) ? QSizeF(160, 160) : QSizeF(200, 150);
            r = QRectF(m_start - QPointF(size.width() / 2, size.height() / 2), size);
        }
        shape = ShapeObject::createBox(kind, r, style, s.polygonSides());
    }
    const QRectF dirty = geom::inflated(QRectF(m_start, m_current).normalized(), 80);
    std::vector<ObjectPtr> objects;
    objects.push_back(std::move(shape));
    host().document().commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects),
                                                                          QObject::tr("Draw %1").arg(shapeKindLabel(kind).toLower())));
    host().updateOverlay(dirty);
}

void ShapeTool::finishPolygon()
{
    Page* page = host().page();
    if (page && m_polygon.size() >= 3) {
        std::vector<ObjectPtr> objects;
        objects.push_back(ShapeObject::createPolygon(m_polygon, host().settings().shapeStyle()));
        host().document().commands().push(std::make_unique<AddObjectsCommand>(page->id(), std::move(objects),
                                                                              QObject::tr("Draw polygon")));
    }
    m_polygon.clear();
    host().updateOverlayAll();
}

void ShapeTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    host().updateOverlayAll();
}

void ShapeTool::hover(const PointerEvent& e)
{
    if (!m_polygon.isEmpty()) {
        m_hover = e.pagePos;
        host().updateOverlayAll();
    }
}

bool ShapeTool::keyPress(QKeyEvent* event)
{
    if (m_polygon.isEmpty())
        return false;
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finishPolygon();
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        m_polygon.clear();
        host().updateOverlayAll();
        return true;
    }
    if (event->key() == Qt::Key_Backspace) {
        m_polygon.removeLast();
        host().updateOverlayAll();
        return true;
    }
    return false;
}

void ShapeTool::deactivate()
{
    if (m_polygon.size() >= 3)
        finishPolygon();
    m_polygon.clear();
    m_pointer = -1;
}

void ShapeTool::pageChanged()
{
    m_polygon.clear();
    m_pointer = -1;
}

void ShapeTool::paintOverlay(QPainter& p) const
{
    const ToolSettings& s = host().settings();
    const ShapeStyle style = s.shapeStyle();
    QPen pen(style.stroke, style.width, style.dashed ? Qt::DashLine : Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (!m_polygon.isEmpty()) {
        QPolygonF poly(m_polygon);
        poly << (m_pointer >= 0 ? m_current : m_hover);
        p.setPen(pen);
        p.setBrush(style.fill.alpha() > 0 ? QBrush(style.fill) : Qt::NoBrush);
        p.drawPolyline(poly);
        const qreal r = host().viewToPageLength(host().theme().dp(7));
        p.setBrush(style.stroke);
        for (const QPointF& v : m_polygon)
            p.drawEllipse(v, r, r);
        if (m_polygon.size() >= 3) {
            QPen hint(host().theme().color(ThemeColor::Accent), host().viewToPageLength(2.0), Qt::DashLine);
            p.setPen(hint);
            p.setBrush(Qt::NoBrush);
            const qreal rr = host().viewToPageLength(host().theme().dp(22));
            p.drawEllipse(m_polygon.first(), rr, rr);
        }
    } else if (m_pointer >= 0) {
        const ShapeKind kind = s.shapeKind();
        if (isLineShape(kind)) {
            ShapeObject::paintArrow(p, m_start, lineEnd(m_mods), pen, kind == ShapeKind::DoubleArrow,
                                    kind != ShapeKind::Line);
        } else if (kind != ShapeKind::FreePolygon) {
            const QRectF r = dragRect(m_mods);
            p.translate(r.center());
            p.setPen(pen);
            p.setBrush(style.fill.alpha() > 0 ? QBrush(style.fill) : Qt::NoBrush);
            p.drawPath(ShapeObject::outlineFor(kind, r.size(), s.polygonSides()));
        }
    }
    p.restore();
}

} // namespace cb
