#pragma once

#include "document/DocumentObject.h"
#include "tools/Tool.h"

#include <QObject>

#include <functional>

namespace cb {

class GraphObject;

/// Interacts with function graphs: drag pans the view, pinch zooms it, a tap selects the graph
/// or (in point mode) places a point, snapped onto a nearby curve or grid coordinate.
class GraphTool final : public QObject, public Tool
{
    Q_OBJECT
public:
    explicit GraphTool(ToolHost& host);

    ToolId id() const override { return ToolId::Graph; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    bool gesture(const GestureEvent& e, const QPointF& pageCentroid) override;
    void pageChanged() override;
    QCursor cursor() const override { return QCursor(Qt::OpenHandCursor); }

    void setPointMode(bool on) { m_pointMode = on; }
    bool pointMode() const { return m_pointMode; }

    /// The graph the tool works on (selected graph, else the topmost graph under the pointer).
    GraphObject* targetGraph(const QPointF& pagePos) const;

    /// Applies a modification to a graph as one undo step.
    static void modifyGraph(ToolHost& host, const ObjectId& id, const QString& text,
                            const std::function<void(GraphObject&)>& change);

signals:
    void graphChanged(const QUuid& id);

private:
    void beginChange(GraphObject* graph);
    void commitChange(const QString& text);

    ObjectId m_graph;
    ObjectPtr m_before;
    int m_pointer = -1;
    QPointF m_pressPage;
    QPointF m_lastPage;
    bool m_moved = false;
    bool m_pointMode = false;
};

} // namespace cb
