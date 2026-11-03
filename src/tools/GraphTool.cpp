#include "tools/GraphTool.h"

#include "canvas/ViewTransform.h"
#include "core/Geometry.h"
#include "document/Commands.h"
#include "document/Document.h"
#include "graph/GraphObject.h"
#include "tools/SelectionModel.h"
#include "ui/Theme.h"

#include <cmath>

namespace cb {

GraphTool::GraphTool(ToolHost& host)
    : QObject(nullptr)
    , Tool(host)
{
}

GraphObject* GraphTool::targetGraph(const QPointF& pagePos) const
{
    Page* page = host().page();
    if (!page)
        return nullptr;
    const auto& objects = page->objects();
    for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
        if ((*it)->type() == ObjectType::Graph && (*it)->hitTest(pagePos, 0.0))
            return static_cast<GraphObject*>(it->get());
    }
    return nullptr;
}

void GraphTool::modifyGraph(ToolHost& host, const ObjectId& id, const QString& text,
                            const std::function<void(GraphObject&)>& change)
{
    Page* page = host.page();
    DocumentObject* o = page ? page->object(id) : nullptr;
    if (!o || o->type() != ObjectType::Graph)
        return;
    ObjectPtr before = o->clone();
    ObjectPtr after = o->clone();
    change(static_cast<GraphObject&>(*after));
    auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), text);
    cmd->add(std::move(before), std::move(after));
    host.document().commands().push(std::move(cmd));
}

void GraphTool::beginChange(GraphObject* graph)
{
    m_graph = graph->id();
    m_before = graph->clone();
}

void GraphTool::commitChange(const QString& text)
{
    Page* page = host().page();
    DocumentObject* current = page ? page->object(m_graph) : nullptr;
    if (current && m_before) {
        auto cmd = std::make_unique<ModifyObjectsCommand>(page->id(), text);
        cmd->add(std::move(m_before), current->clone());
        host().document().commands().pushApplied(std::move(cmd));
        emit graphChanged(m_graph);
    }
    m_before.reset();
}

void GraphTool::pointerDown(const PointerEvent& e)
{
    if (m_pointer != -1)
        return;
    GraphObject* graph = targetGraph(e.pagePos);
    if (!graph)
        return;
    m_pointer = e.pointerId;
    m_pressPage = e.pagePos;
    m_lastPage = e.pagePos;
    m_moved = false;
    host().selection().setSingle(graph->id());
    beginChange(graph);
}

void GraphTool::pointerMove(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    if (!m_moved && geom::distance(host().view().pageToView(e.pagePos), host().view().pageToView(m_pressPage))
            < host().theme().dp(6))
        return;
    m_moved = true;
    Page* page = host().page();
    DocumentObject* o = page ? page->object(m_graph) : nullptr;
    if (!o)
        return;
    ObjectPtr c = o->clone();
    auto* g = static_cast<GraphObject*>(c.get());
    const QPointF localDelta = g->mapFromPage(e.pagePos) - g->mapFromPage(m_lastPage);
    g->panLocal(localDelta);
    m_lastPage = e.pagePos;
    host().document().replaceObject(page->id(), std::move(c));
}

void GraphTool::pointerUp(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    if (m_moved) {
        commitChange(QObject::tr("Pan graph"));
        return;
    }
    // Tap.
    Page* page = host().page();
    DocumentObject* o = page ? page->object(m_graph) : nullptr;
    if (!o || !m_pointMode) {
        m_before.reset();
        emit graphChanged(m_graph);
        return;
    }
    ObjectPtr c = o->clone();
    auto* g = static_cast<GraphObject*>(c.get());
    const QPointF local = g->mapFromPage(e.pagePos);
    const qreal tolLocal = host().viewToPageLength(host().theme().dp(18));
    const int existing = g->pointAt(local, tolLocal);
    if (existing >= 0) {
        g->removePoint(existing);
    } else {
        QPointF m = g->localToMath(local);
        // Snap to a curve if one passes close by, else to whole grid coordinates.
        bool onCurve = false;
        for (int i = 0; i < static_cast<int>(g->functions().size()); ++i) {
            if (!g->functions()[static_cast<size_t>(i)].visible)
                continue;
            const double y = g->evaluate(i, m.x());
            if (std::isfinite(y) && geom::distance(g->mathToLocal(QPointF(m.x(), y)), local) <= tolLocal) {
                m.setY(y);
                onCurve = true;
                break;
            }
        }
        if (!onCurve) {
            const QPointF rounded(std::round(m.x()), std::round(m.y()));
            if (geom::distance(g->mathToLocal(rounded), local) <= tolLocal)
                m = rounded;
        }
        g->addPoint(m);
    }
    host().document().replaceObject(page->id(), std::move(c));
    commitChange(existing >= 0 ? QObject::tr("Remove point") : QObject::tr("Add point"));
}

void GraphTool::pointerCancel(const PointerEvent& e)
{
    if (e.pointerId != m_pointer)
        return;
    m_pointer = -1;
    Page* page = host().page();
    if (page && m_before)
        host().document().replaceObject(page->id(), m_before->clone());
    m_before.reset();
}

bool GraphTool::gesture(const GestureEvent& e, const QPointF& pageCentroid)
{
    switch (e.phase) {
    case GesturePhase::Begin: {
        GraphObject* graph = targetGraph(pageCentroid);
        if (!graph)
            return false;
        host().selection().setSingle(graph->id());
        beginChange(graph);
        return true;
    }
    case GesturePhase::Update: {
        Page* page = host().page();
        DocumentObject* o = page ? page->object(m_graph) : nullptr;
        if (!o)
            return true;
        ObjectPtr c = o->clone();
        auto* g = static_cast<GraphObject*>(c.get());
        const QPointF local = g->mapFromPage(pageCentroid);
        g->panLocal(e.panDelta / host().view().zoom());
        g->zoomAtLocal(local, e.scaleDelta);
        host().document().replaceObject(page->id(), std::move(c));
        return true;
    }
    case GesturePhase::End:
        commitChange(QObject::tr("Zoom graph"));
        return true;
    case GesturePhase::Cancel: {
        Page* page = host().page();
        if (page && m_before)
            host().document().replaceObject(page->id(), m_before->clone());
        m_before.reset();
        return true;
    }
    }
    return false;
}

void GraphTool::deactivate()
{
    m_pointer = -1;
    m_before.reset();
}

void GraphTool::pageChanged()
{
    m_pointer = -1;
    m_before.reset();
}

} // namespace cb
