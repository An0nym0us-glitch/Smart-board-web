#pragma once

#include "input/InputEvent.h"

#include <QElapsedTimer>
#include <QHash>
#include <QPointF>
#include <QVector>

class QEvent;
class QMouseEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;

namespace cb {

/// Receiver of normalised input.
class InputSink
{
public:
    virtual ~InputSink() = default;
    virtual void pointerEvent(PointerEvent& event) = 0;
    virtual void gestureEvent(const GestureEvent& event) = 0;
    virtual void wheelZoom(const QPointF& viewPos, qreal factor) = 0;
    virtual void wheelPan(const QPointF& viewDelta) = 0;
    /// Middle/right mouse button drag.
    virtual void dragPan(const QPointF& viewDelta) = 0;
};

/// Converts Qt mouse, tablet and touch events into device independent pointer events and
/// recognises multi-touch gestures (pan/zoom and the grouped palm/wipe eraser).
///
/// Ten-point touch. Every contact is tracked by its own touch id and given one role:
///  * write    -> every finger on its own draws with the active tool, independently of the
///                others (as many fingers as the panel reports, typically 10 or 20)
///  * gesture  -> a second finger landing while the first finger's line has only just started
///                and nobody is really writing: two fingers of one hand (within kPinchMaxSpanMm),
///                or one finger of each hand landing almost together. That line is cancelled
///                and the fingers pan / zoom; further fingers near the gesture join it
///  * palm     -> three fingers held tightly together (see PalmGesture.h), or one very large
///                contact: the wipe eraser; its fingers never draw, others keep writing
///  * ignored  -> fingers left over from a gesture, other fingers of the wiping hand, fingers
///                landing while the view moves
///  * "every finger draws" mode (setMultiUserTouch) turns two-finger pan / zoom off

///  * pen priority        -> while the stylus touches or hovers over the board (and briefly after),
///                           new touches are ignored so a resting hand neither draws nor
///                           palm-erases; a young finger stroke is cancelled when the pen lands
class InputManager
{
public:
    explicit InputManager(InputSink& sink);

    /// Handles an event delivered to the canvas widget. Returns true if consumed.
    bool handleEvent(QEvent* event);

    void setPalmEraseEnabled(bool enabled) { m_palmEnabled = enabled; }
    bool palmEraseEnabled() const { return m_palmEnabled; }
    /// "Every finger draws": turns two-finger pan / zoom off (fingers close together still draw).
    void setMultiUserTouch(bool enabled) { m_multiUser = enabled; }
    bool multiUserTouch() const { return m_multiUser; }
    /// Screen density used to convert physical thresholds (mm) to pixels.
    void setPixelsPerMm(qreal ppm) { m_pixelsPerMm = ppm > 0.5 ? ppm : 3.78; }
    qreal pixelsPerMm() const { return m_pixelsPerMm; }

    /// Cancels every active pointer / gesture (e.g. when the page changes).
    void cancelAll();

    /// True while the stylus touches the board or was seen within kStylusProximityMs.
    bool stylusActive() const;
    /// Touch input state, for diagnostics and tests.
    /// PalmErase is reported while a wipe is in progress (in multi-user mode other fingers may
    /// still be drawing at the same time).
    enum class TouchState { Idle, Drawing, MultiDrawing, PanZoom, PalmErase, Ignoring };
    TouchState touchState() const;
    /// Touch ids of the fingers forming the active wipe (empty when there is none).
    QVector<int> palmTouchIds() const { return m_palmIds; }

    static constexpr qint64 kStylusProximityMs = 400;

    static constexpr int kMousePointerId = 0;
    static constexpr int kStylusPointerId = 1;
    static constexpr int kTouchPointerBase = 100;

private:
    enum class Role { Write, Gesture, Palm, Ignored };

    struct Track
    {
        QPointF start;
        QPointF pos;
        qint64 startTime = 0;
        qreal diameter = 0.0;
        Role role = Role::Ignored;
    };

    bool handleMouse(QMouseEvent* e);
    bool handleTablet(QTabletEvent* e);
    bool handleTouch(QTouchEvent* e);
    bool handleWheel(QWheelEvent* e);

    void forwardTouch(int id, PointerPhase phase, const Track& t, Qt::KeyboardModifiers mods);
    void classifyNewTouch(int id, qint64 now, Qt::KeyboardModifiers mods);
    void cancelWriting(int id);
    int countRole(Role role) const;
    bool isYoung(const Track& t, qint64 now) const;
    int findPinchPartner(int newId, qint64 now) const;
    void beginPanZoom();
    void updatePanZoom();
    void endPanZoom(GesturePhase phase);
    QVector<int> findPalmGroup(int newId, qint64 now) const;
    void beginPalm(const QVector<int>& ids);
    void updatePalm(GesturePhase phase);
    void endPalm();
    bool palmActive() const { return !m_palmIds.isEmpty(); }
    QPointF palmCentroid() const;
    void rebaseline();
    QPointF centroid() const;
    qreal spread(const QPointF& c) const;
    qreal angle(const QPointF& c) const;
    qreal palmRadius(const QPointF& c) const;
    bool isPalmContact(const Track& t) const;

    InputSink& m_sink;
    bool m_palmEnabled = true;
    bool m_multiUser = false;
    qreal m_pixelsPerMm = 3.78;

    // Mouse
    bool m_mouseDown = false;
    bool m_mousePanning = false;
    QPointF m_lastMousePos;

    // Stylus
    bool m_stylusDown = false;
    PointerDevice m_stylusDevice = PointerDevice::Stylus;
    qint64 m_lastStylusTime = -1;

    // Touch
    QElapsedTimer m_clock;
    QHash<int, Track> m_tracks;
    bool m_panZoom = false;
    QVector<int> m_palmIds; ///< fingers of the active wipe; they never produce pointer events
    QPointF m_prevPalmCentroid;
    QPointF m_prevCentroid;
    qreal m_prevSpread = 0.0;
    qreal m_prevAngle = 0.0;
};

} // namespace cb
