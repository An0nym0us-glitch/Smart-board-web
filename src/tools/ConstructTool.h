#pragma once

#include "tools/Tool.h"

namespace cb {

/// Places geometric constructions: points (tap) and segments, lines, rays and vectors (drag),
/// snapping to existing points and to whole coordinates.
class ConstructTool final : public Tool
{
public:
    explicit ConstructTool(ToolHost& host);

    ToolId id() const override { return ToolId::Construct; }
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void hover(const PointerEvent& e) override;
    void paintOverlay(QPainter& painter) const override;
    void paintViewOverlay(QPainter& painter) const override;
    void pageChanged() override;

private:
    int m_pointer = -1;
    QPointF m_start;
    QPointF m_current;
    bool m_snapped = false;
    bool m_hovering = false;
    int m_nameCounter = 0;
};

} // namespace cb
