#pragma once

#include "core/Id.h"
#include "input/InputEvent.h"

#include <QCursor>
#include <QRectF>

class QKeyEvent;
class QPainter;
class QWidget;

namespace cb {

class Document;
class Page;
class ViewTransform;
class SelectionModel;
class ToolSettings;
class Theme;
class InstrumentLayer;
class MagicHighlightLayer;

enum class ToolId {
    Pen,
    Eraser,
    Select,
    Shape,
    Text,
    Measure,
    Construct,
    Graph,
};

/// Services the canvas offers to tools. Tools never talk to Qt widgets directly.
class ToolHost
{
public:
    virtual ~ToolHost() = default;
    virtual Document& document() = 0;
    virtual Page* page() const = 0;
    virtual const ViewTransform& view() const = 0;
    virtual SelectionModel& selection() = 0;
    virtual ToolSettings& settings() = 0;
    virtual const Theme& theme() const = 0;
    virtual InstrumentLayer& instruments() = 0;
    /// Temporary effect layer (Magic Highlighter): never part of the document or the undo stack.
    virtual MagicHighlightLayer& magicHighlights() = 0;
    virtual QWidget* widget() = 0;

    /// Schedules a repaint of the overlay covering a page-space rect.
    virtual void updateOverlay(const QRectF& pageRect) = 0;
    virtual void updateOverlayAll() = 0;
    /// Excludes an object from normal rendering (e.g. while edited inline).
    virtual void setObjectHidden(const ObjectId& id, bool hidden) = 0;
    /// Asks the application to open the editor of an object (text, equation, graph, table).
    virtual void requestEdit(const ObjectId& id) = 0;
    /// Converts a length in view pixels (already UI scaled) to page units.
    virtual qreal viewToPageLength(qreal viewPixels) const = 0;
    /// UI scale factor for touch-friendly handle sizes.
    virtual qreal uiScale() const = 0;
};

/// Base class of canvas tools. Tools receive device independent pointer events and may draw a
/// transient overlay. They modify the document only through commands.
class Tool
{
public:
    explicit Tool(ToolHost& host)
        : m_host(host)
    {
    }
    virtual ~Tool() = default;
    Tool(const Tool&) = delete;
    Tool& operator=(const Tool&) = delete;

    virtual ToolId id() const = 0;
    virtual void activate() {}
    /// Must finish or cancel any pending interaction.
    virtual void deactivate() {}

    virtual void pointerDown(const PointerEvent& e) = 0;
    virtual void pointerMove(const PointerEvent& e) = 0;
    virtual void pointerUp(const PointerEvent& e) = 0;
    virtual void pointerCancel(const PointerEvent& e) = 0;
    virtual void hover(const PointerEvent& e) { Q_UNUSED(e); }

    /// Overlay painting with the painter in page coordinates.
    virtual void paintOverlay(QPainter& painter) const { Q_UNUSED(painter); }
    /// Overlay painting with the painter in view coordinates (handles, labels).
    virtual void paintViewOverlay(QPainter& painter) const { Q_UNUSED(painter); }

    virtual bool keyPress(QKeyEvent* event)
    {
        Q_UNUSED(event);
        return false;
    }
    /// Offers a two-finger gesture to the tool (e.g. zooming inside a graph). pageCentroid is the
    /// gesture centre in page coordinates at Begin. Return true to consume the whole gesture.
    virtual bool gesture(const GestureEvent& event, const QPointF& pageCentroid)
    {
        Q_UNUSED(event);
        Q_UNUSED(pageCentroid);
        return false;
    }
    /// Called when the displayed page changes; pending work must be committed or dropped.
    virtual void pageChanged() {}
    virtual QCursor cursor() const { return QCursor(Qt::CrossCursor); }

protected:
    ToolHost& host() const { return m_host; }

private:
    ToolHost& m_host;
};

} // namespace cb
