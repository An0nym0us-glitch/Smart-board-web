#pragma once

#include "document/DocumentObject.h"
#include "tools/Tool.h"

#include <QElapsedTimer>
#include <QPolygonF>

#include <vector>

namespace cb {

/// Selection, move, resize (single object in its own frame, uniform scale for groups), rotate,
/// control point editing, rubber-band and lasso selection. Every drag is one undo step.
class SelectTool final : public Tool
{
public:
    explicit SelectTool(ToolHost& host);

    ToolId id() const override { return ToolId::Select; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void hover(const PointerEvent& e) override;
    void paintViewOverlay(QPainter& painter) const override;
    bool keyPress(QKeyEvent* event) override;
    void pageChanged() override;
    QCursor cursor() const override { return QCursor(Qt::ArrowCursor); }

    /// Page-space bounds of the current selection (null if empty).
    QRectF selectionBounds() const;

private:
    enum class Mode { None, Pending, Move, Resize, Rotate, ControlPoint, RubberBand, Lasso };

    struct Original
    {
        ObjectId id;
        ObjectPtr object;
    };

    struct Frame
    {
        bool valid = false;
        bool single = false;
        QPolygonF viewCorners;  ///< TL, TR, BR, BL in view coordinates
        QPointF rotateHandle;   ///< view
        bool canResize = false;
        bool canRotate = false;
        bool keepAspect = false;
        QVector<QPointF> controlPoints; ///< view
    };

    Frame frame() const;
    std::vector<DocumentObject*> selectedObjects() const;
    QVector<QPointF> resizeHandles(const Frame& f) const; ///< 8 handles (view), corners at even indices
    int hitResizeHandle(const Frame& f, const QPointF& view) const;

    void startDrag(Mode mode, const PointerEvent& e);
    void updateMove(const QPointF& pagePos);
    void updateResize(const QPointF& pagePos, Qt::KeyboardModifiers mods);
    void updateRotate(const QPointF& pagePos);
    void updateControlPoint(const QPointF& pagePos);
    void finishDrag();
    void cancelDrag();
    void finishBand(const PointerEvent& e);
    void handleTap(const PointerEvent& e);
    void nudge(const QPointF& delta);

    Mode m_mode = Mode::None;
    int m_pointer = -1;
    QPointF m_pressPage;
    QPointF m_pressView;
    ObjectId m_pressedObject;
    bool m_pressedWasSelected = false;
    std::vector<Original> m_originals;
    int m_handle = -1;
    QRectF m_startBounds;        ///< page AABB of the selection at drag start
    QPointF m_pivot;             ///< rotation centre / scale anchor (page)
    qreal m_startAngle = 0.0;
    QRectF m_band;               ///< page
    QPolygonF m_lasso;           ///< page
    QElapsedTimer m_tapTimer;
    QPointF m_lastTapView;
    ObjectId m_lastTapObject;
};

} // namespace cb
