#include "tools/ToolController.h"

#include <QPainter>
#include <QSet>

namespace cb {

ToolController::ToolController(ToolHost& host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
}

ToolController::~ToolController() = default;

void ToolController::registerTool(std::unique_ptr<Tool> tool)
{
    const ToolId id = tool->id();
    m_tools[id] = std::move(tool);
}

Tool* ToolController::tool(ToolId id) const
{
    const auto it = m_tools.find(id);
    return it == m_tools.end() ? nullptr : it->second.get();
}

Tool* ToolController::activeTool() const
{
    return tool(m_active);
}

void ToolController::setActiveTool(ToolId id)
{
    if (!tool(id))
        return;
    if (id == m_active) {
        emit activeToolChanged(id);
        return;
    }
    if (Tool* old = activeTool())
        old->deactivate();
    m_active = id;
    activeTool()->activate();
    m_host.updateOverlayAll();
    emit activeToolChanged(id);
}

void ToolController::pointerEvent(const PointerEvent& e)
{
    Tool* target = nullptr;
    switch (e.phase) {
    case PointerPhase::Down: {
        target = (e.device == PointerDevice::StylusEraser) ? tool(ToolId::Eraser) : activeTool();
        if (!target)
            return;
        m_owners.insert(e.pointerId, target);
        target->pointerDown(e);
        return;
    }
    case PointerPhase::Move:
        target = m_owners.value(e.pointerId, nullptr);
        if (target)
            target->pointerMove(e);
        return;
    case PointerPhase::Up:
        target = m_owners.take(e.pointerId);
        if (target)
            target->pointerUp(e);
        return;
    case PointerPhase::Cancel:
        target = m_owners.take(e.pointerId);
        if (target)
            target->pointerCancel(e);
        return;
    case PointerPhase::Hover:
        target = (e.device == PointerDevice::StylusEraser) ? tool(ToolId::Eraser) : activeTool();
        if (target)
            target->hover(e);
        return;
    }
}

void ToolController::paintOverlay(QPainter& painter) const
{
    // The active tool plus any tool that still owns a pointer (e.g. stylus eraser).
    QSet<Tool*> painted;
    if (Tool* t = activeTool()) {
        t->paintOverlay(painter);
        painted.insert(t);
    }
    for (Tool* t : m_owners) {
        if (!painted.contains(t)) {
            t->paintOverlay(painter);
            painted.insert(t);
        }
    }
}

void ToolController::paintViewOverlay(QPainter& painter) const
{
    QSet<Tool*> painted;
    if (Tool* t = activeTool()) {
        t->paintViewOverlay(painter);
        painted.insert(t);
    }
    for (Tool* t : m_owners) {
        if (!painted.contains(t)) {
            t->paintViewOverlay(painter);
            painted.insert(t);
        }
    }
}

bool ToolController::keyPress(QKeyEvent* event)
{
    Tool* t = activeTool();
    return t && t->keyPress(event);
}

void ToolController::pageChanged()
{
    cancelAll();
    for (auto& entry : m_tools)
        entry.second->pageChanged();
}

void ToolController::cancelAll()
{
    const auto owners = m_owners;
    m_owners.clear();
    for (auto it = owners.constBegin(); it != owners.constEnd(); ++it) {
        PointerEvent e;
        e.pointerId = it.key();
        e.phase = PointerPhase::Cancel;
        it.value()->pointerCancel(e);
    }
}

} // namespace cb
