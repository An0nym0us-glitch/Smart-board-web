#include "tools/EraserTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Document.h"
#include "document/ShapeObject.h"
#include "document/StrokeObject.h"
#include "ui/Theme.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cb {

EraserTool::EraserTool(ToolHost& host)
    : Tool(host)
{
}

qreal EraserTool::pageRadius() const
{
    return host().viewToPageLength(host().settings().eraserSize() * host().uiScale() * 0.5);
}

void EraserTool::beginSession(EraserMode mode)
{
    if (m_session.active) {
        ++m_session.users;
        return;
    }
    Page* page = host().page();
    if (!page)
        return;
    m_session = Session();
    m_session.active = true;
    m_session.page = page->id();
    m_session.mode = mode;
    m_session.users = 1;
    for (int i = 0; i < page->objectCount(); ++i)
        m_session.initialIndex.insert(page->objectAt(i)->id(), i);
}

void EraserTool::takeOut(const ObjectId& id)
{
    Document& doc = host().document();
    ObjectPtr taken = doc.takeObject(m_session.page, id);
    if (!taken)
        return;
    if (m_session.created.remove(id))
        return; // a fragment created in this session simply disappears
    ReplaceObjectsCommand::Removed r;
    r.index = m_session.initialIndex.value(id, 0);
    r.object = std::move(taken);
    m_session.removed.push_back(std::move(r));
}

EraserTool::Method EraserTool::methodFor(EraserMode mode)
{
    switch (mode) {
    case EraserMode::Stroke: return Method::Stroke;
    case EraserMode::Object: return Method::Object;
    case EraserMode::Area: return Method::Area;
    }
    return Method::Area;
}

bool EraserTool::cutStroke(const ObjectId& id, const QPointF& center, qreal radius)
{
    Page* page = host().page();
    auto* stroke = static_cast<StrokeObject*>(page->object(id));
    bool touched = false;
    auto fragments = stroke->eraseCircle(center, radius, &touched);
    if (!touched)
        return false;
    const int index = page->indexOf(id);
    takeOut(id);
    int insertAt = index;
    for (auto& frag : fragments) {
        m_session.created.insert(frag->id());
        host().document().insertObject(m_session.page, insertAt++, std::move(frag));
    }
    return true;
}

bool EraserTool::cutShape(const ObjectId& id, const QPointF& center, qreal radius)
{
    Page* page = host().page();
    const auto* shape = static_cast<const ShapeObject*>(page->object(id));
    if (!shape->hitTest(center, radius))
        return false;
    // A shape smaller than the eraser (an arrowhead, a dot) simply goes.
    const QRectF b = shape->sceneBounds();
    if (std::max(b.width(), b.height()) <= 2.0 * radius) {
        takeOut(id);
        return true;
    }
    // Otherwise it becomes ink in the same colour and width, and the part under the eraser is
    // rubbed out like any other ink: a triangle loses one side, a circle gets a gap.
    auto pieces = shape->toInk();
    const int index = page->indexOf(id);
    takeOut(id);
    std::vector<ObjectId> ids;
    int insertAt = index;
    for (auto& piece : pieces) {
        ids.push_back(piece->id());
        m_session.created.insert(piece->id());
        host().document().insertObject(m_session.page, insertAt++, std::move(piece));
    }
    for (const ObjectId& pid : ids) {
        DocumentObject* o = page->object(pid);
        if (!o)
            continue;
        if (o->type() == ObjectType::Stroke)
            cutStroke(pid, center, radius);
        else if (o->hitTest(center, radius))
            takeOut(pid); // arrowhead under the eraser
    }
    return true;
}

void EraserTool::eraseAt(const QPointF& center, qreal radius, Method method)
{
    if (!m_session.active)
        return;
    Page* page = host().page();
    if (!page || page->id() != m_session.page)
        return;
    const QRectF area(center.x() - radius, center.y() - radius, 2 * radius, 2 * radius);
    // Collect ids first: the page is modified while erasing.
    std::vector<ObjectId> candidates;
    for (DocumentObject* o : page->objectsIntersecting(area))
        candidates.push_back(o->id());

    for (const ObjectId& id : candidates) {
        DocumentObject* o = page->object(id);
        if (!o)
            continue;
        switch (method) {
        case Method::Area:
            if (o->type() == ObjectType::Stroke)
                cutStroke(id, center, radius);
            else if (o->type() == ObjectType::Shape)
                cutShape(id, center, radius);
            break;
        case Method::Stroke:
            if (o->type() == ObjectType::Stroke && o->hitTest(center, radius))
                takeOut(id);
            break;
        case Method::Object:
            if (o->hitTest(center, radius))
                takeOut(id);
            break;
        case Method::Wipe:
            if (o->type() == ObjectType::Stroke)
                cutStroke(id, center, radius);
            else if (o->type() == ObjectType::Shape)
                cutShape(id, center, radius);
            else if (o->type() != ObjectType::Image && o->hitTest(center, radius))
                takeOut(id);
            break;
        }
    }
}

void EraserTool::eraseAlong(const QPointF& from, const QPointF& to, qreal radius, Method method)
{
    const qreal dist = geom::distance(from, to);
    const qreal step = std::max(radius * 0.5, host().viewToPageLength(1.0));
    const int n = std::max(1, static_cast<int>(std::ceil(dist / step)));
    for (int i = 1; i <= n; ++i)
        eraseAt(geom::lerp(from, to, double(i) / n), radius, method);
}

void EraserTool::commit()
{
    if (!m_session.active)
        return;
    Session s = std::move(m_session);
    m_session = Session();
    if (s.removed.empty() && s.created.isEmpty())
        return;
    Page* page = host().document().pageById(s.page);
    std::vector<ReplaceObjectsCommand::Added> added;
    if (page) {
        for (const ObjectId& id : s.created) {
            const int index = page->indexOf(id);
            if (index >= 0)
                added.push_back({id, index});
        }
    }
    host().document().commands().pushApplied(
        std::make_unique<ReplaceObjectsCommand>(s.page, std::move(s.removed), std::move(added), QObject::tr("Erase")));
}

void EraserTool::pointerDown(const PointerEvent& e)
{
    beginSession(host().settings().eraserMode());
    m_pointers.insert(e.pointerId, e.pagePos);
    m_hoverView = e.viewPos;
    m_hoverVisible = true;
    eraseAt(e.pagePos, pageRadius(), methodFor(m_session.mode));
    host().updateOverlayAll();
}

void EraserTool::pointerMove(const PointerEvent& e)
{
    auto it = m_pointers.find(e.pointerId);
    if (it == m_pointers.end())
        return;
    eraseAlong(*it, e.pagePos, pageRadius(), methodFor(m_session.mode));
    *it = e.pagePos;
    m_hoverView = e.viewPos;
    host().updateOverlayAll();
}

void EraserTool::pointerUp(const PointerEvent& e)
{
    if (!m_pointers.remove(e.pointerId))
        return;
    if (--m_session.users <= 0)
        commit();
    m_hoverVisible = e.device == PointerDevice::Mouse;
    host().updateOverlayAll();
}

void EraserTool::pointerCancel(const PointerEvent& e)
{
    // Keep what was erased so far: it is consistent and undoable.
    pointerUp(e);
}

void EraserTool::hover(const PointerEvent& e)
{
    m_hoverView = e.viewPos;
    m_hoverVisible = true;
    host().updateOverlayAll();
}

void EraserTool::deactivate()
{
    m_pointers.clear();
    commit();
    m_hoverVisible = false;
}

void EraserTool::pageChanged()
{
    m_pointers.clear();
    commit();
    m_palmActive = false;
}

void EraserTool::beginPalm(const QPointF& center, qreal radius)
{
    if (m_palmActive)
        return;
    // Joins a running eraser session (pen erasing at the same time): one undo step for both.
    beginSession(host().settings().eraserMode());
    if (!m_session.active)
        return;
    m_palmActive = true;
    m_palmCenter = center;
    eraseAt(center, radius, Method::Wipe);
}

void EraserTool::movePalm(const QPointF& center, qreal radius)
{
    if (!m_palmActive)
        return;
    eraseAlong(m_palmCenter, center, radius, Method::Wipe);
    m_palmCenter = center;
}

void EraserTool::endPalm()
{
    if (!m_palmActive)
        return;
    m_palmActive = false;
    if (--m_session.users <= 0)
        commit();
}

void EraserTool::paintViewOverlay(QPainter& painter) const
{
    if (!m_hoverVisible)
        return;
    const Theme& t = host().theme();
    const qreal r = host().settings().eraserSize() * host().uiScale() * 0.5;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 0, 0, 120), t.dp(3)));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(m_hoverView, r, r);
    painter.setPen(QPen(QColor(255, 255, 255, 220), t.dp(1.5)));
    painter.setBrush(QColor(255, 255, 255, m_pointers.isEmpty() ? 18 : 40));
    painter.drawEllipse(m_hoverView, r, r);
    painter.restore();
}

} // namespace cb
