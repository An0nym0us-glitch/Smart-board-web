#pragma once

#include "tools/Tool.h"

#include <QVector>

namespace cb {

/// Distance, angle, slope and area measurement, snapping to construction points and grid
/// coordinates. Results are live MeasurementObjects on the page.
///  * distance / slope: drag from A to B
///  * angle: drag from the vertex along the first arm, then tap the end of the second arm
///  * area: tap the vertices, tap the first vertex (or double tap / Enter) to close
class MeasureTool final : public Tool
{
public:
    explicit MeasureTool(ToolHost& host);

    ToolId id() const override { return ToolId::Measure; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void hover(const PointerEvent& e) override;
    void paintOverlay(QPainter& painter) const override;
    void paintViewOverlay(QPainter& painter) const override;
    bool keyPress(QKeyEvent* event) override;
    void pageChanged() override;

private:
    void commit();
    void reset();
    QVector<QPointF> previewPoints() const;

    QVector<QPointF> m_points;
    QPointF m_current;
    bool m_currentSnapped = false;
    int m_pointer = -1;
    bool m_dragging = false;
    qint64 m_lastTapMs = 0;
};

} // namespace cb
