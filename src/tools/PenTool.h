#pragma once

#include "document/StrokeObject.h"
#include "geometry/Instrument.h"
#include "tools/Tool.h"

#include <QHash>

namespace cb {

/// Freehand ink (pen, highlighter, dashed/dotted) with input smoothing, stylus pressure,
/// drawing along instruments and several simultaneous pointers (multi-user touch).
///
/// In Magic Highlighter mode the same live stroke is drawn, but when it is finished it is handed
/// to the canvas' temporary MagicHighlightLayer instead of being added to the document: no
/// object, no undo step.
class PenTool final : public Tool
{
public:
    explicit PenTool(ToolHost& host);

    ToolId id() const override { return ToolId::Pen; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void paintOverlay(QPainter& painter) const override;
    void paintViewOverlay(QPainter& painter) const override;
    void pageChanged() override;
    QCursor cursor() const override;

private:
    struct LiveStroke
    {
        QVector<StrokePoint> points;
        QPointF smoothed;
        InkStyle ink;
        EdgeConstraint constraint;
        QPointF start;
        bool magic = false; ///< Magic Highlighter stroke (temporary)
    };

    void addPoint(LiveStroke& s, const QPointF& raw, qreal pressure, bool force);
    void finish(int pointerId);
    QRectF strokeRect(const LiveStroke& s, int fromIndex) const;

    QHash<int, LiveStroke> m_live;
};

} // namespace cb
