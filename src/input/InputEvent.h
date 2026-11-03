#pragma once

#include <QPointF>
#include <Qt>

namespace cb {

enum class PointerDevice {
    Mouse,
    Touch,
    Stylus,
    StylusEraser,
};

enum class PointerPhase {
    Down,
    Move,
    Up,
    Cancel,
    Hover,
};

/// Device independent pointer event produced by the InputManager.
struct PointerEvent
{
    int pointerId = 0;
    PointerDevice device = PointerDevice::Mouse;
    PointerPhase phase = PointerPhase::Down;
    QPointF viewPos;        ///< widget coordinates
    QPointF pagePos;        ///< page coordinates (filled in by the canvas)
    qreal pressure = 1.0;   ///< 0..1, 1 for devices without pressure
    bool hasPressure = false;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    unsigned long timestamp = 0;
};

enum class GestureType {
    PanZoom,   ///< two or three finger pan / pinch zoom / rotate
    PalmErase, ///< deliberate multi-finger (or palm) wipe
};

enum class GesturePhase {
    Begin,
    Update,
    End,
    Cancel,
};

/// Recognised multi-touch gesture. Deltas are incremental since the previous update.
struct GestureEvent
{
    GestureType type = GestureType::PanZoom;
    GesturePhase phase = GesturePhase::Begin;
    QPointF centroid;          ///< view coordinates
    QPointF panDelta;          ///< view pixels
    qreal scaleDelta = 1.0;    ///< multiplicative
    qreal rotationDelta = 0.0; ///< degrees
    qreal radius = 0.0;        ///< palm erase radius in view pixels
    int touchCount = 0;
};

} // namespace cb
