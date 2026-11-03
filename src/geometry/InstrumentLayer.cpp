#include "geometry/InstrumentLayer.h"

#include "core/Geometry.h"
#include "geometry/Instruments.h"

#include <QPainter>

#include <limits>

namespace cb {

InstrumentLayer::InstrumentLayer(QObject* parent)
    : QObject(parent)
{
}

InstrumentLayer::~InstrumentLayer() = default;

Instrument* InstrumentLayer::find(Instrument::Kind kind) const
{
    for (const auto& i : m_instruments)
        if (i->kind() == kind)
            return i.get();
    return nullptr;
}

void InstrumentLayer::show(Instrument::Kind kind, const QPointF& pagePos)
{
    if (find(kind))
        return;
    std::unique_ptr<Instrument> inst = createInstrument(kind);
    if (!inst)
        return;
    inst->setPixelsPerUnit(m_pxPerUnit);
    inst->setUnitLabel(m_unitLabel);
    inst->setPosition(pagePos);
    m_instruments.push_back(std::move(inst));
    emit visibilityChanged();
    emit changed();
}

void InstrumentLayer::hide(Instrument::Kind kind)
{
    for (auto it = m_instruments.begin(); it != m_instruments.end(); ++it) {
        if ((*it)->kind() == kind) {
            Instrument* raw = it->get();
            for (auto o = m_owners.begin(); o != m_owners.end();) {
                if (o.value() == raw)
                    o = m_owners.erase(o);
                else
                    ++o;
            }
            m_instruments.erase(it);
            emit visibilityChanged();
            emit changed();
            return;
        }
    }
}

void InstrumentLayer::toggle(Instrument::Kind kind, const QPointF& pagePos)
{
    if (find(kind))
        hide(kind);
    else
        show(kind, pagePos);
}

void InstrumentLayer::setUnits(qreal pxPerUnit, const QString& label)
{
    m_pxPerUnit = pxPerUnit;
    m_unitLabel = label;
    for (auto& i : m_instruments) {
        i->setPixelsPerUnit(pxPerUnit);
        i->setUnitLabel(label);
    }
    emit changed();
}

Instrument* InstrumentLayer::instrumentAt(const QPointF& pagePos, qreal tol) const
{
    for (auto it = m_instruments.rbegin(); it != m_instruments.rend(); ++it) {
        Instrument* inst = it->get();
        if (inst->handleAt(inst->toLocal(pagePos), tol) != Instrument::kNoHandle)
            return inst;
    }
    return nullptr;
}

int InstrumentLayer::handleAt(const QPointF& pagePos, qreal tol) const
{
    for (auto it = m_instruments.rbegin(); it != m_instruments.rend(); ++it) {
        const int handle = (*it)->handleAt((*it)->toLocal(pagePos), tol);
        if (handle != Instrument::kNoHandle)
            return handle;
    }
    return Instrument::kNoHandle;
}

bool InstrumentLayer::hitTest(const QPointF& pagePos, qreal tol) const
{
    return instrumentAt(pagePos, tol) != nullptr;
}

bool InstrumentLayer::pointerDown(int pointerId, const QPointF& pagePos, qreal tol)
{
    for (auto it = m_instruments.rbegin(); it != m_instruments.rend(); ++it) {
        Instrument* inst = it->get();
        if (inst->isInteracting())
            continue;
        const int handle = inst->handleAt(inst->toLocal(pagePos), tol);
        if (handle == Instrument::kNoHandle)
            continue;
        if (handle == Instrument::kCloseHandle) {
            // Swallow the pointer; the instrument is removed on release.
            m_owners.insert(pointerId, inst);
            inst->beginInteraction(handle, pagePos);
            return true;
        }
        if (!inst->beginInteraction(handle, pagePos))
            return false;
        m_owners.insert(pointerId, inst);
        // Bring to front.
        std::unique_ptr<Instrument> moved = std::move(*it);
        m_instruments.erase(std::next(it).base());
        m_instruments.push_back(std::move(moved));
        emit changed();
        return true;
    }
    return false;
}

void InstrumentLayer::pointerMove(int pointerId, const QPointF& pagePos)
{
    Instrument* inst = m_owners.value(pointerId, nullptr);
    if (!inst)
        return;
    if (inst->activeHandle() == Instrument::kCloseHandle)
        return;
    inst->updateInteraction(pagePos);
    emit statusText(inst->statusText());
    emit changed();
}

InstrumentResult InstrumentLayer::pointerUp(int pointerId)
{
    Instrument* inst = m_owners.take(pointerId);
    if (!inst)
        return {};
    if (inst->activeHandle() == Instrument::kCloseHandle) {
        inst->cancelInteraction();
        hide(inst->kind());
        emit statusText(QString());
        return {};
    }
    InstrumentResult r = inst->endInteraction();
    emit statusText(QString());
    emit changed();
    return r;
}

void InstrumentLayer::pointerCancel(int pointerId)
{
    Instrument* inst = m_owners.take(pointerId);
    if (!inst)
        return;
    inst->cancelInteraction();
    emit statusText(QString());
    emit changed();
}

bool InstrumentLayer::edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const
{
    bool found = false;
    qreal best = std::numeric_limits<qreal>::max();
    for (const auto& inst : m_instruments) {
        EdgeConstraint c;
        if (inst->edgeConstraint(pagePos, tol, &c)) {
            const qreal d = c.distanceTo(pagePos);
            if (d < best) {
                best = d;
                if (out)
                    *out = c;
                found = true;
            }
        }
    }
    return found;
}

void InstrumentLayer::paint(QPainter& painter, qreal zoom, const CoordinateSystem* coordinates) const
{
    for (const auto& inst : m_instruments) {
        InstrumentPaintContext ctx;
        ctx.zoom = zoom;
        ctx.coordinates = coordinates;
        ctx.active = inst->isInteracting();
        painter.save();
        painter.setTransform(inst->transform(), true);
        inst->paint(painter, ctx);
        painter.restore();
    }
}

QRectF InstrumentLayer::sceneBounds() const
{
    QRectF r;
    for (const auto& inst : m_instruments)
        r = r.isNull() ? inst->sceneBounds() : r.united(inst->sceneBounds());
    return r;
}

} // namespace cb
