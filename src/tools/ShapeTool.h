#pragma once

#include "tools/Tool.h"

#include <QPointF>
#include <QVector>

namespace cb {

/// Draws shapes: drag for box and line shapes (Shift keeps proportions / 45° angles), tap for a
/// default sized shape, and tap-by-tap vertices for free polygons (tap the first vertex, double
/// tap or press Enter to finish).
class ShapeTool final : public Tool
{
public:
    explicit ShapeTool(ToolHost& host);

    ToolId id() const override { return ToolId::Shape; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void hover(const PointerEvent& e) override;
    void paintOverlay(QPainter& painter) const override;
    bool keyPress(QKeyEvent* event) override;
    void pageChanged() override;

private:
    QRectF dragRect(Qt::KeyboardModifiers mods) const;
    QPointF lineEnd(Qt::KeyboardModifiers mods) const;
    void finishPolygon();

    int m_pointer = -1;
    QPointF m_start;
    QPointF m_current;
    Qt::KeyboardModifiers m_mods = Qt::NoModifier;
    QVector<QPointF> m_polygon;
    QPointF m_hover;
    qint64 m_lastTapMs = 0;
};

} // namespace cb
