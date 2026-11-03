#pragma once

#include "document/Commands.h"
#include "tools/Tool.h"
#include "tools/ToolSettings.h"

#include <QHash>
#include <QSet>

#include <vector>

namespace cb {

/// Stroke, object and area eraser, plus the grouped palm / wipe eraser gesture.
///
/// An erase "session" lasts while any eraser pointer (or the wipe) is down; all changes of a
/// session become a single undo step (ReplaceObjectsCommand).
///
/// The area eraser rubs out exactly what is under it, ink and shapes alike: a shape it touches
/// is turned into ink of the same colour and width (ShapeObject::toInk) and only the part under
/// the eraser disappears (one side of a triangle, a gap in a circle). A fill is dropped then.
///
/// The wipe erases like the board eraser of a real whiteboard: ink and shapes under it are cut
/// away like the area eraser, and every other object it touches (equations, text, graphs,
/// constructions, measurements, tables) is removed using the objects' own hit tests.
/// Pictures and imported PDF / PowerPoint pages are left alone so a wipe over annotations never
/// removes the slide underneath.
class EraserTool final : public Tool
{
public:
    explicit EraserTool(ToolHost& host);

    ToolId id() const override { return ToolId::Eraser; }
    void deactivate() override;
    void pointerDown(const PointerEvent& e) override;
    void pointerMove(const PointerEvent& e) override;
    void pointerUp(const PointerEvent& e) override;
    void pointerCancel(const PointerEvent& e) override;
    void hover(const PointerEvent& e) override;
    void paintViewOverlay(QPainter& painter) const override;
    void pageChanged() override;
    QCursor cursor() const override { return QCursor(Qt::BlankCursor); }

    // Wipe gesture (page coordinates). Works whatever tool is active.
    void beginPalm(const QPointF& center, qreal radius);
    void movePalm(const QPointF& center, qreal radius);
    void endPalm();

private:
    struct Session
    {
        bool active = false;
        PageId page;
        EraserMode mode = EraserMode::Area;
        QHash<ObjectId, int> initialIndex;             ///< page order before the session
        std::vector<ReplaceObjectsCommand::Removed> removed;
        QSet<ObjectId> created;
        int users = 0;
    };

    /// How a single erase step treats the objects under it.
    enum class Method { Stroke, Object, Area, Wipe };
    static Method methodFor(EraserMode mode);

    void beginSession(EraserMode mode);
    void eraseAt(const QPointF& center, qreal radius, Method method);
    void eraseAlong(const QPointF& from, const QPointF& to, qreal radius, Method method);
    bool cutStroke(const ObjectId& id, const QPointF& center, qreal radius);
    bool cutShape(const ObjectId& id, const QPointF& center, qreal radius);
    void takeOut(const ObjectId& id);
    void commit();
    qreal pageRadius() const;

    Session m_session;
    QHash<int, QPointF> m_pointers; ///< last page position per pointer
    QPointF m_hoverView;
    bool m_hoverVisible = false;
    QPointF m_palmCenter;
    bool m_palmActive = false;
};

} // namespace cb
