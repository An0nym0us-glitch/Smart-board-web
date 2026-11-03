#include "input/InputManager.h"

#include "core/Geometry.h"
#include "input/PalmGesture.h"

#include <QMouseEvent>
#include <QSet>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <vector>

namespace cb {

namespace {
// A second finger turns a stroke into pan/zoom only while the stroke is "young": it has barely
// moved (a finger resting before a pinch), or it started a moment ago and is still short (two
// fingers of a pinch landing a few frames apart). A line that is really being drawn is never
// interrupted.
// Real hands do not land two fingers in the same instant: the first finger may already slide a
// little (and IR touch frames jitter by a few millimetres) before the second one arrives.
constexpr qreal kYoungStrokeMm = 12.0;     // moved less than this: always young
constexpr qint64 kYoungStrokeMs = 350;     // started less than this ago ...
constexpr qreal kYoungStrokeMaxMm = 30.0;  // ... and moved less than this: young
// Ten-point touch: every finger writes on its own. Two fingers of ONE hand (closer than this)
// zoom and pan instead, while nobody else is writing; fingers further apart are different
// people writing at the same time.
constexpr qreal kPinchMaxSpanMm = 160.0;
// Zooming / dragging with one finger of EACH hand: both fingers land almost together (within
// this window) and not further apart than this. Two people rarely start in the same instant.
constexpr qint64 kTwoHandWindowMs = 200;
constexpr qreal kTwoHandMaxSpanMm = 450.0;
// While zooming, a finger this close to the gesture centre joins it (three-finger pan).
constexpr qreal kGestureJoinMm = 160.0;
// While wiping, other fingers this close to the wipe belong to the same hand and are ignored.
constexpr qreal kWipeHandReachMm = 160.0;
// The wipe eraser thresholds are in input/PalmGesture.h.
} // namespace

InputManager::InputManager(InputSink& sink)
    : m_sink(sink)
{
    m_clock.start();
}

bool InputManager::handleEvent(QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseMove:
    case QEvent::MouseButtonDblClick:
        return handleMouse(static_cast<QMouseEvent*>(event));
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
        return handleTablet(static_cast<QTabletEvent*>(event));
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        return handleTouch(static_cast<QTouchEvent*>(event));
    case QEvent::Wheel:
        return handleWheel(static_cast<QWheelEvent*>(event));
    default:
        return false;
    }
}

void InputManager::cancelAll()
{
    if (m_mouseDown) {
        PointerEvent pe;
        pe.pointerId = kMousePointerId;
        pe.phase = PointerPhase::Cancel;
        pe.viewPos = m_lastMousePos;
        m_sink.pointerEvent(pe);
        m_mouseDown = false;
    }
    m_mousePanning = false;
    if (m_stylusDown) {
        PointerEvent pe;
        pe.pointerId = kStylusPointerId;
        pe.device = m_stylusDevice;
        pe.phase = PointerPhase::Cancel;
        m_sink.pointerEvent(pe);
        m_stylusDown = false;
    }
    for (auto it = m_tracks.begin(); it != m_tracks.end(); ++it)
        if (it->role == Role::Write)
            forwardTouch(it.key(), PointerPhase::Cancel, *it, Qt::NoModifier);
    endPanZoom(GesturePhase::Cancel); // the view is restored
    endPalm();                        // what was wiped so far stays (one undo step)
    m_tracks.clear();
}

bool InputManager::stylusActive() const
{
    return m_stylusDown || (m_lastStylusTime >= 0 && m_clock.elapsed() - m_lastStylusTime < kStylusProximityMs);
}

InputManager::TouchState InputManager::touchState() const
{
    if (m_tracks.isEmpty())
        return TouchState::Idle;
    if (palmActive())
        return TouchState::PalmErase;
    if (m_panZoom)
        return TouchState::PanZoom;
    const int writers = countRole(Role::Write);
    if (writers == 1)
        return TouchState::Drawing;
    if (writers > 1)
        return TouchState::MultiDrawing;
    return TouchState::Ignoring;
}

// ------------------------------------------------------------------------------------------ mouse

bool InputManager::handleMouse(QMouseEvent* e)
{
    // Touch and pen input are handled natively; ignore the mouse events the OS/Qt synthesises.
    if (e->source() != Qt::MouseEventNotSynthesized) {
        e->accept();
        return true;
    }
    const QPointF pos = e->localPos();
    PointerEvent pe;
    pe.pointerId = kMousePointerId;
    pe.device = PointerDevice::Mouse;
    pe.viewPos = pos;
    pe.modifiers = e->modifiers();
    pe.timestamp = e->timestamp();

    switch (e->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
        if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
            m_mousePanning = true;
            m_lastMousePos = pos;
        } else if (e->button() == Qt::LeftButton && !m_mouseDown) {
            m_mouseDown = true;
            pe.phase = PointerPhase::Down;
            m_sink.pointerEvent(pe);
        }
        break;
    case QEvent::MouseMove:
        if (m_mousePanning) {
            m_sink.dragPan(pos - m_lastMousePos);
        } else {
            pe.phase = m_mouseDown ? PointerPhase::Move : PointerPhase::Hover;
            m_sink.pointerEvent(pe);
        }
        break;
    case QEvent::MouseButtonRelease:
        if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
            m_mousePanning = false;
        } else if (e->button() == Qt::LeftButton && m_mouseDown) {
            m_mouseDown = false;
            pe.phase = PointerPhase::Up;
            m_sink.pointerEvent(pe);
        }
        break;
    default:
        break;
    }
    m_lastMousePos = pos;
    e->accept();
    return true;
}

// ----------------------------------------------------------------------------------------- stylus

bool InputManager::handleTablet(QTabletEvent* e)
{
    PointerEvent pe;
    pe.pointerId = kStylusPointerId;
    pe.device = e->pointerType() == QTabletEvent::Eraser ? PointerDevice::StylusEraser : PointerDevice::Stylus;
    pe.viewPos = e->posF();
    pe.pressure = std::clamp(e->pressure(), 0.0, 1.0);
    pe.hasPressure = true;
    pe.modifiers = e->modifiers();
    pe.timestamp = e->timestamp();
    m_lastStylusTime = m_clock.elapsed();

    switch (e->type()) {
    case QEvent::TabletPress:
        if (!m_stylusDown) {
            // Pen priority: a hand that touched the board just before the pen must not leave ink or
            // keep zooming. Young finger strokes are cancelled; the fingers are ignored until lifted.
            const QList<int> ids = m_tracks.keys();
            for (int id : ids)
                cancelWriting(id);
            endPanZoom(GesturePhase::End);
            endPalm();
            for (Track& t : m_tracks)
                t.role = Role::Ignored;
            m_stylusDown = true;
            m_stylusDevice = pe.device;
            pe.phase = PointerPhase::Down;
            m_sink.pointerEvent(pe);
        }
        break;
    case QEvent::TabletMove:
        if (m_stylusDown) {
            pe.device = m_stylusDevice;
            pe.phase = PointerPhase::Move;
        } else {
            pe.phase = PointerPhase::Hover;
        }
        m_sink.pointerEvent(pe);
        break;
    case QEvent::TabletRelease:
        if (m_stylusDown) {
            m_stylusDown = false;
            pe.device = m_stylusDevice;
            pe.phase = PointerPhase::Up;
            m_sink.pointerEvent(pe);
        }
        break;
    default:
        break;
    }
    e->accept();
    return true;
}

// ------------------------------------------------------------------------------------------ wheel

bool InputManager::handleWheel(QWheelEvent* e)
{
    const QPoint angle = e->angleDelta();
    const QPoint pixels = e->pixelDelta();
    if (e->modifiers() & Qt::ShiftModifier) {
        const int d = angle.y() != 0 ? angle.y() : angle.x();
        m_sink.wheelPan(QPointF(d * 0.6, 0));
    } else if (!pixels.isNull() && !(e->modifiers() & Qt::ControlModifier)) {
        m_sink.wheelPan(QPointF(pixels));
    } else if (angle.y() != 0) {
        m_sink.wheelZoom(e->position(), std::pow(1.0015, angle.y()));
    }
    e->accept();
    return true;
}

// ------------------------------------------------------------------------------------------ touch

void InputManager::forwardTouch(int id, PointerPhase phase, const Track& t, Qt::KeyboardModifiers mods)
{
    PointerEvent pe;
    pe.pointerId = kTouchPointerBase + id;
    pe.device = PointerDevice::Touch;
    pe.phase = phase;
    pe.viewPos = t.pos;
    pe.modifiers = mods;
    pe.timestamp = static_cast<unsigned long>(m_clock.elapsed());
    m_sink.pointerEvent(pe);
}

void InputManager::cancelWriting(int id)
{
    auto it = m_tracks.find(id);
    if (it == m_tracks.end() || it->role != Role::Write)
        return;
    forwardTouch(id, PointerPhase::Cancel, *it, Qt::NoModifier);
    it->role = Role::Ignored;
}

int InputManager::countRole(Role role) const
{
    int n = 0;
    for (const Track& t : m_tracks)
        n += t.role == role;
    return n;
}

bool InputManager::isYoung(const Track& t, qint64 now) const
{
    const qreal moved = geom::distance(t.start, t.pos);
    return moved < kYoungStrokeMm * m_pixelsPerMm
        || ((now - t.startTime) < kYoungStrokeMs && moved < kYoungStrokeMaxMm * m_pixelsPerMm);
}

QPointF InputManager::centroid() const
{
    QPointF c;
    int n = 0;
    for (const Track& t : m_tracks) {
        if (t.role != Role::Gesture)
            continue;
        c += t.pos;
        ++n;
    }
    return n > 0 ? c / n : m_prevCentroid;
}

qreal InputManager::spread(const QPointF& c) const
{
    qreal s = 0.0;
    int n = 0;
    for (const Track& t : m_tracks) {
        if (t.role != Role::Gesture)
            continue;
        s += geom::distance(t.pos, c);
        ++n;
    }
    return n > 0 ? s / n : 0.0;
}

qreal InputManager::angle(const QPointF& c) const
{
    Q_UNUSED(c);
    QList<int> ids;
    for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it)
        if (it->role == Role::Gesture)
            ids << it.key();
    if (ids.size() < 2)
        return 0.0;
    std::sort(ids.begin(), ids.end());
    return geom::angleDeg(m_tracks.value(ids[1]).pos - m_tracks.value(ids[0]).pos);
}

QPointF InputManager::palmCentroid() const
{
    QPointF c;
    int n = 0;
    for (int id : m_palmIds) {
        auto it = m_tracks.constFind(id);
        if (it == m_tracks.constEnd())
            continue;
        c += it->pos;
        ++n;
    }
    return n > 0 ? c / n : m_prevPalmCentroid;
}

qreal InputManager::palmRadius(const QPointF& c) const
{
    qreal r = PALM_ERASER_MIN_RADIUS_MM * m_pixelsPerMm;
    for (int id : m_palmIds) {
        auto it = m_tracks.constFind(id);
        if (it != m_tracks.constEnd())
            r = std::max(r, geom::distance(it->pos, c) + it->diameter * 0.5 + PALM_ERASER_RADIUS_MARGIN_MM * m_pixelsPerMm);
    }
    return r;
}

bool InputManager::isPalmContact(const Track& t) const
{
    return t.diameter >= PALM_ERASER_PALM_CONTACT_MM * m_pixelsPerMm;
}

QVector<int> InputManager::findPalmGroup(int newId, qint64 now) const
{
    // The new finger plus the nearest fingers that landed with it (within the group window) and
    // have not travelled yet. Only if those are packed tightly together is it a wipe; fingers
    // spread apart stay writing / pinching.
    if (!m_palmEnabled || palmActive() || PALM_ERASER_FINGER_COUNT < 2)
        return {};
    auto added = m_tracks.constFind(newId);
    if (added == m_tracks.constEnd())
        return {};
    struct Candidate
    {
        int id;
        qreal distance;
    };
    std::vector<Candidate> candidates;
    for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
        if (it.key() == newId || it->role == Role::Ignored)
            continue;
        if (now - it->startTime > PALM_ERASER_GROUP_WINDOW_MS)
            continue;
        if (geom::distance(it->start, it->pos) > PALM_ERASER_MAX_TRAVEL_MM * m_pixelsPerMm)
            continue;
        candidates.push_back({it.key(), geom::distance(it->pos, added->pos)});
    }
    const int needed = PALM_ERASER_FINGER_COUNT - 1;
    if (static_cast<int>(candidates.size()) < needed)
        return {};
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    QVector<int> ids{newId};
    QVector<QPointF> positions{added->pos};
    for (int i = 0; i < needed; ++i) {
        ids << candidates[i].id;
        positions << m_tracks.value(candidates[i].id).pos;
    }
    if (!isTightPalmGroup(measurePalmGroup(positions), m_pixelsPerMm))
        return {};
    return ids;
}

int InputManager::findPinchPartner(int newId, qint64 now) const
{
    // A second finger turns into pan / zoom together with a finger whose line has only just
    // started: two fingers of one hand (close together), or one finger of each hand landing
    // almost at the same moment. Never while somebody is really writing (moving the view would
    // bend their line). Fingers further apart, or arriving later, are different people writing.
    if (m_multiUser || palmActive() || m_panZoom)
        return -1;
    auto added = m_tracks.constFind(newId);
    if (added == m_tracks.constEnd())
        return -1;
    int partner = -1;
    qreal best = 0.0;
    for (auto it = m_tracks.constBegin(); it != m_tracks.constEnd(); ++it) {
        if (it.key() == newId || it->role != Role::Write)
            continue;
        if (!isYoung(*it, now))
            return -1; // a line is really being drawn
        const qreal d = geom::distance(it->pos, added->pos);
        const bool oneHand = d <= kPinchMaxSpanMm * m_pixelsPerMm;
        const bool twoHands = d <= kTwoHandMaxSpanMm * m_pixelsPerMm && added->startTime - it->startTime <= kTwoHandWindowMs;
        if ((oneHand || twoHands) && (partner < 0 || d < best)) {
            partner = it.key();
            best = d;
        }
    }
    return partner;
}

void InputManager::rebaseline()
{
    m_prevCentroid = centroid();
    m_prevSpread = spread(m_prevCentroid);
    m_prevAngle = angle(m_prevCentroid);
}

void InputManager::beginPanZoom()
{
    m_panZoom = true;
    rebaseline();
    GestureEvent g;
    g.type = GestureType::PanZoom;
    g.phase = GesturePhase::Begin;
    g.centroid = m_prevCentroid;
    g.touchCount = countRole(Role::Gesture);
    m_sink.gestureEvent(g);
}

void InputManager::updatePanZoom()
{
    const QPointF c = centroid();
    const qreal s = spread(c);
    const qreal a = angle(c);
    const int n = countRole(Role::Gesture);
    GestureEvent g;
    g.type = GestureType::PanZoom;
    g.phase = GesturePhase::Update;
    g.centroid = c;
    g.panDelta = c - m_prevCentroid;
    g.scaleDelta = (m_prevSpread > 8.0 && s > 8.0) ? s / m_prevSpread : 1.0;
    g.rotationDelta = n >= 2 ? geom::angleDifference(m_prevAngle, a) : 0.0;
    g.touchCount = n;
    m_prevCentroid = c;
    m_prevSpread = s;
    m_prevAngle = a;
    m_sink.gestureEvent(g);
}

void InputManager::endPanZoom(GesturePhase phase)
{
    if (!m_panZoom)
        return;
    GestureEvent g;
    g.type = GestureType::PanZoom;
    g.phase = phase;
    g.centroid = m_prevCentroid;
    m_sink.gestureEvent(g);
    m_panZoom = false;
    // Fingers left over from the gesture never start drawing.
    for (Track& t : m_tracks)
        if (t.role == Role::Gesture)
            t.role = Role::Ignored;
}

void InputManager::beginPalm(const QVector<int>& ids)
{
    // One gesture at a time: a wipe replaces a pan / zoom that was just starting.
    endPanZoom(GesturePhase::Cancel);
    // Fingers of the group that had started a line leave no ink.
    for (int id : ids) {
        cancelWriting(id);
        auto it = m_tracks.find(id);
        if (it != m_tracks.end())
            it->role = Role::Palm;
    }
    m_palmIds = ids;
    updatePalm(GesturePhase::Begin);
}

void InputManager::updatePalm(GesturePhase phase)
{
    GestureEvent g;
    g.type = GestureType::PalmErase;
    g.phase = phase;
    g.centroid = palmCentroid();
    g.radius = palmRadius(g.centroid);
    g.touchCount = m_palmIds.size();
    m_prevPalmCentroid = g.centroid;
    m_sink.gestureEvent(g);
}

void InputManager::endPalm()
{
    if (!palmActive())
        return;
    updatePalm(GesturePhase::End);
    for (int id : m_palmIds) {
        auto it = m_tracks.find(id);
        if (it != m_tracks.end())
            it->role = Role::Ignored;
    }
    m_palmIds.clear();
}

void InputManager::classifyNewTouch(int id, qint64 now, Qt::KeyboardModifiers mods)
{
    Track& t = m_tracks[id];
    t.role = Role::Ignored;
    if (stylusActive())
        return; // pen priority: the writing hand rests on the board
    const QVector<int> group = findPalmGroup(id, now);
    if (!group.isEmpty()) {
        beginPalm(group);
        return;
    }
    if (m_palmEnabled && !palmActive() && isPalmContact(t)) {
        beginPalm({id});
        return;
    }
    if (m_panZoom) {
        // Another finger of the zooming hand joins; anyone else waits until the view is still.
        if (geom::distance(t.pos, centroid()) <= kGestureJoinMm * m_pixelsPerMm) {
            t.role = Role::Gesture;
            rebaseline();
        }
        return;
    }
    if (palmActive() && geom::distance(t.pos, palmCentroid()) <= kWipeHandReachMm * m_pixelsPerMm)
        return; // thumb or palm of the wiping hand
    const int partner = findPinchPartner(id, now);
    if (partner >= 0) {
        // Lines that had only just started (the first finger of the gesture, a resting finger)
        // leave no ink; the view is about to move under them.
        const QList<int> ids = m_tracks.keys();
        for (int other : ids)
            cancelWriting(other);
        m_tracks[partner].role = Role::Gesture;
        m_tracks[id].role = Role::Gesture;
        beginPanZoom();
        return;
    }
    // A finger of its own: it writes (every finger independently, up to the panel's limit).
    t.role = Role::Write;
    forwardTouch(id, PointerPhase::Down, t, mods);
}

bool InputManager::handleTouch(QTouchEvent* e)
{
    e->accept();
    if (e->type() == QEvent::TouchCancel) {
        cancelAll();
        return true;
    }
    const qint64 now = m_clock.elapsed();
    const Qt::KeyboardModifiers mods = e->modifiers();
    const auto& points = e->touchPoints();

    // 1. Update positions of known tracks.
    QSet<int> moved;
    for (const QTouchEvent::TouchPoint& tp : points) {
        auto it = m_tracks.find(tp.id());
        if (it == m_tracks.end())
            continue;
        if (it->pos != tp.pos()) {
            it->pos = tp.pos();
            moved.insert(tp.id());
        }
        const QSizeF d = tp.ellipseDiameters();
        it->diameter = std::max(it->diameter, std::max(d.width(), d.height()));
    }

    // 2. New contacts, each tracked by its own touch id.
    for (const QTouchEvent::TouchPoint& tp : points) {
        if (tp.state() != Qt::TouchPointPressed || m_tracks.contains(tp.id()))
            continue;
        Track t;
        t.start = tp.pos();
        t.pos = tp.pos();
        t.startTime = now;
        const QSizeF d = tp.ellipseDiameters();
        t.diameter = std::max(d.width(), d.height());
        m_tracks.insert(tp.id(), t);
        classifyNewTouch(tp.id(), now, mods);
    }

    // 3. Movement.
    if (!moved.isEmpty()) {
        bool gestureMoved = false;
        bool palmMoved = false;
        for (const QTouchEvent::TouchPoint& tp : points) {
            if (!moved.contains(tp.id()))
                continue;
            auto it = m_tracks.constFind(tp.id());
            if (it == m_tracks.constEnd())
                continue;
            switch (it->role) {
            case Role::Write: forwardTouch(tp.id(), PointerPhase::Move, *it, mods); break;
            case Role::Gesture: gestureMoved = true; break;
            case Role::Palm: palmMoved = true; break;
            case Role::Ignored: break;
            }
        }
        if (gestureMoved && m_panZoom)
            updatePanZoom();
        if (palmMoved)
            updatePalm(GesturePhase::Update);
    }

    // 4. Released contacts.
    for (const QTouchEvent::TouchPoint& tp : points) {
        if (tp.state() != Qt::TouchPointReleased)
            continue;
        auto it = m_tracks.find(tp.id());
        if (it == m_tracks.end())
            continue;
        Track t = *it;
        t.pos = tp.pos();
        m_tracks.erase(it);
        switch (t.role) {
        case Role::Write:
            forwardTouch(tp.id(), PointerPhase::Up, t, mods);
            break;
        case Role::Gesture:
            if (countRole(Role::Gesture) < 2)
                endPanZoom(GesturePhase::End);
            else
                rebaseline(); // no jump: the remaining fingers continue smoothly
            break;
        case Role::Palm:
            // The wipe stays active until every finger of the group is lifted.
            if (m_palmIds.removeAll(tp.id()) > 0 && m_palmIds.isEmpty())
                updatePalm(GesturePhase::End);
            break;
        case Role::Ignored:
            break;
        }
    }
    if (e->type() == QEvent::TouchEnd && !m_tracks.isEmpty()) {
        // Some drivers end the sequence without releasing every point.
        cancelAll();
    }
    return true;
}

} // namespace cb
