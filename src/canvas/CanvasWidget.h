#pragma once

#include "canvas/ViewTransform.h"
#include "input/InputManager.h"
#include "tools/Tool.h"

#include <QHash>
#include <QPixmap>
#include <QRegion>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <memory>

namespace cb {

struct UiContext;
class Document;
class ToolController;
class ToolSettings;
class SelectionModel;
class InstrumentLayer;
class MagicHighlightLayer;
class EraserTool;
class Instrument;
struct InstrumentResult;

/// The board. Renders the current page through a cached layer with dirty-region updates, draws
/// tool and instrument overlays, and turns device input into tool actions and gestures.
class CanvasWidget : public QWidget, public ToolHost, public InputSink
{
    Q_OBJECT
public:
    CanvasWidget(const UiContext& ui, Document& doc, ToolSettings& settings, QWidget* parent = nullptr);
    ~CanvasWidget() override;

    ToolController& tools() { return *m_tools; }
    SelectionModel& selectionModel() { return *m_selection; }
    InputManager& input() { return m_input; }

    // View control. Zoom only changes the view transform: document coordinates, measurements and
    // exports are never affected. 100 % = one document unit per device independent pixel.
    void fitPage();
    /// Fits the page width into the view (page top at the top of the view).
    void fitWidth();
    void zoomBy(qreal factor);
    /// Sets an exact zoom (1.0 = 100 %) keeping the centre of the view in place.
    void setZoom(qreal zoom);
    /// Moves to the next larger (direction > 0) or smaller zoom preset.
    void zoomStep(int direction);
    static const QVector<qreal>& zoomPresets();
    qreal zoom() const { return m_view.zoom(); }
    QPointF viewCenterInPage() const;
    QRect pageRectToWidget(const QRectF& pageRect) const;
    void setShowPageFrame(bool on);
    /// Pans so that a page rect is visible (used after pasting / inserting).
    void ensureVisible(const QRectF& pageRect);
    /// Page position of the most recent pointer press (used to find the tapped table cell).
    QPointF lastPressPage() const { return m_lastPress; }
    /// Touch density in logical pixels per millimetre for a screen (used for gesture distances).
    static qreal pixelsPerMm(const QSize& logicalSize, const QSizeF& physicalSizeMm);

    // ToolHost
    Document& document() override { return m_doc; }
    Page* page() const override;
    const ViewTransform& view() const override { return m_view; }
    SelectionModel& selection() override { return *m_selection; }
    ToolSettings& settings() override { return m_settings; }
    const Theme& theme() const override;
    InstrumentLayer& instruments() override { return *m_instruments; }
    MagicHighlightLayer& magicHighlights() override { return *m_magic; }
    QWidget* widget() override { return this; }
    void updateOverlay(const QRectF& pageRect) override;
    void updateOverlayAll() override { update(); }
    void setObjectHidden(const ObjectId& id, bool hidden) override;
    void requestEdit(const ObjectId& id) override;
    qreal viewToPageLength(qreal viewPixels) const override { return viewPixels / m_view.zoom(); }
    qreal uiScale() const override;

signals:
    void zoomChanged(qreal zoom);
    void viewChanged();
    void editRequested(const QUuid& objectId);
    void statusText(const QString& text);
    /// A pointer interaction (stroke, drag, tap) has finished.
    void interactionFinished();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    // InputSink
    void pointerEvent(PointerEvent& event) override;
    void gestureEvent(const GestureEvent& event) override;
    void wheelZoom(const QPointF& viewPos, qreal factor) override;
    void wheelPan(const QPointF& viewDelta) override;
    void dragPan(const QPointF& viewDelta) override;

    void onObjectAdded(const QUuid& pageId, const QUuid& objectId, const QRectF& bounds);
    void onObjectRemoved(const QUuid& pageId, const QUuid& objectId, const QRectF& bounds);
    void onObjectChanged(const QUuid& pageId, const QUuid& objectId, const QRectF& oldBounds, const QRectF& newBounds);
    void onPageChanged(const QUuid& pageId);
    void onCurrentPageChanged(int index);
    void onDocumentReset();

    void invalidatePageRect(const QRectF& pageRect);
    void invalidateAll();
    void renderDirty();
    void panView(const QPointF& delta);
    void zoomView(const QPointF& viewPos, qreal factor, bool interactive);
    void finishInteractiveZoom();
    void viewUpdated();
    void commitInstrumentResult(InstrumentResult& result);
    void updatePixelsPerMm();

    const UiContext& m_ui;
    Document& m_doc;
    ToolSettings& m_settings;
    InputManager m_input;
    std::unique_ptr<SelectionModel> m_selection;
    std::unique_ptr<InstrumentLayer> m_instruments;
    std::unique_ptr<MagicHighlightLayer> m_magic;
    std::unique_ptr<ToolController> m_tools;
    EraserTool* m_eraser = nullptr;

    ViewTransform m_view;
    QHash<QUuid, ViewTransform> m_pageViews;
    PageId m_viewPage;
    bool m_viewInitialised = false;

    QPixmap m_cache;
    ViewTransform m_cacheView;
    QRegion m_dirty;
    bool m_interactiveZoom = false;
    QTimer m_zoomSettle;
    QSet<ObjectId> m_hidden;
    bool m_showFrame = false;

    // Gestures
    enum class GestureTarget { None, Canvas, Instrument, Tool, Palm };
    GestureTarget m_gestureTarget = GestureTarget::None;
    Instrument* m_gestureInstrument = nullptr;
    ViewTransform m_gestureStartView;
    QPointF m_palmCenter;
    qreal m_palmRadius = 0.0;
    QSet<int> m_instrumentPointers;
    QPointF m_lastPress;
};

} // namespace cb
