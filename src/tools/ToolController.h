#pragma once

#include "tools/Tool.h"

#include <QHash>
#include <QObject>

#include <map>
#include <memory>

namespace cb {

class EraserTool;

/// Owns the tools and routes pointer events to them.
///
/// Every pointer is bound to the tool that received its Down event, so switching tools while a
/// finger is on the board never delivers half an interaction to another tool. The stylus eraser
/// end is routed to the eraser automatically.
class ToolController : public QObject
{
    Q_OBJECT
public:
    explicit ToolController(ToolHost& host, QObject* parent = nullptr);
    ~ToolController() override;

    void registerTool(std::unique_ptr<Tool> tool);
    Tool* tool(ToolId id) const;
    Tool* activeTool() const;
    ToolId activeToolId() const { return m_active; }
    void setActiveTool(ToolId id);

    void pointerEvent(const PointerEvent& e);
    bool hasActivePointers() const { return !m_owners.isEmpty(); }

    void paintOverlay(QPainter& painter) const;
    void paintViewOverlay(QPainter& painter) const;
    bool keyPress(QKeyEvent* event);
    void pageChanged();
    void cancelAll();

signals:
    void activeToolChanged(cb::ToolId id);

private:
    ToolHost& m_host;
    std::map<ToolId, std::unique_ptr<Tool>> m_tools;
    ToolId m_active = ToolId::Pen;
    QHash<int, Tool*> m_owners;
};

} // namespace cb
