#include "geometry/Instrument.h"

#include "core/Geometry.h"

#include <QPainter>

namespace cb {

QPointF EdgeConstraint::project(const QPointF& p) const
{
    switch (kind) {
    case Kind::Line:
        return geom::projectOntoLine(p, origin, direction);
    case Kind::Circle: {
        QPointF d = p - origin;
        const double len = geom::length(d);
        if (len < 1e-9)
            d = QPointF(radius, 0);
        else
            d = d / len * radius;
        return origin + d;
    }
    case Kind::None:
        break;
    }
    return p;
}

qreal EdgeConstraint::distanceTo(const QPointF& p) const
{
    return geom::distance(p, project(p));
}

void Instrument::setRotation(qreal deg)
{
    m_rotation = geom::normalizeDegrees(deg);
}

QTransform Instrument::transform() const
{
    QTransform t;
    t.translate(m_position.x(), m_position.y());
    t.rotate(m_rotation);
    return t;
}

QPointF Instrument::toLocal(const QPointF& page) const
{
    return geom::rotated(page - m_position, -m_rotation);
}

QRectF Instrument::sceneBounds() const
{
    QRectF local = bodyShape().boundingRect();
    local = local.united(QRectF(closeButtonPos(), QSizeF(1, 1))).united(QRectF(rotateHandlePos(), QSizeF(1, 1)));
    return geom::inflated(transform().mapRect(local), handleRadius() * 2.5);
}

int Instrument::handleAt(const QPointF& local, qreal tol) const
{
    const qreal r = handleRadius() + tol;
    if (geom::distance(local, closeButtonPos()) <= r)
        return kCloseHandle;
    if (geom::distance(local, rotateHandlePos()) <= r)
        return kRotateHandle;
    if (bodyShape().contains(local))
        return kBodyHandle;
    return kNoHandle;
}

bool Instrument::beginInteraction(int handle, const QPointF& pagePos)
{
    m_handle = handle;
    m_grabPage = pagePos;
    m_grabLocal = toLocal(pagePos);
    if (handle == kRotateHandle)
        m_grabAngleOffset = geom::angleDifference(geom::angleDeg(pagePos - m_position), m_rotation);
    return handle != kNoHandle;
}

void Instrument::updateInteraction(const QPointF& pagePos)
{
    switch (m_handle) {
    case kBodyHandle:
        // Keep the grabbed local point under the pointer.
        m_position = pagePos - geom::rotated(m_grabLocal, m_rotation);
        break;
    case kRotateHandle: {
        double angle = geom::angleDeg(pagePos - m_position) + m_grabAngleOffset;
        angle = geom::snapAngle(geom::normalizeDegrees(angle), 15.0, 1.5);
        setRotation(angle);
        break;
    }
    default:
        break;
    }
}

InstrumentResult Instrument::endInteraction()
{
    m_handle = kNoHandle;
    return {};
}

void Instrument::cancelInteraction()
{
    m_handle = kNoHandle;
}

bool Instrument::edgeConstraint(const QPointF& pagePos, qreal tol, EdgeConstraint* out) const
{
    Q_UNUSED(pagePos);
    Q_UNUSED(tol);
    Q_UNUSED(out);
    return false;
}

void Instrument::applyGesture(const QPointF& pageCenter, const QPointF& pageDelta, qreal rotationDelta)
{
    const QPointF rel = m_position - pageCenter;
    m_position = pageCenter + geom::rotated(rel, rotationDelta) + pageDelta;
    setRotation(m_rotation + rotationDelta);
}

void Instrument::paintCloseButton(QPainter& p, const QPointF& c, qreal r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1.0));
    p.setBrush(QColor(40, 44, 46, 230));
    p.drawEllipse(c, r, r);
    QPen cross(QColor(240, 240, 240), r * 0.16, Qt::SolidLine, Qt::RoundCap);
    p.setPen(cross);
    const qreal k = r * 0.38;
    p.drawLine(c + QPointF(-k, -k), c + QPointF(k, k));
    p.drawLine(c + QPointF(-k, k), c + QPointF(k, -k));
    p.restore();
}

void Instrument::paintRotateKnob(QPainter& p, const QPointF& c, qreal r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 0, 0, 60), 1.0));
    p.setBrush(QColor(40, 44, 46, 230));
    p.drawEllipse(c, r, r);
    QPen arc(QColor(240, 240, 240), r * 0.14, Qt::SolidLine, Qt::RoundCap);
    p.setPen(arc);
    p.setBrush(Qt::NoBrush);
    const QRectF box(c.x() - r * 0.5, c.y() - r * 0.5, r, r);
    p.drawArc(box, 30 * 16, 270 * 16);
    // arrow head at the arc end (30 degrees)
    const QPointF tip = c + QPointF(std::cos(geom::degToRad(-30)) * r * 0.5, std::sin(geom::degToRad(-30)) * r * 0.5);
    p.drawLine(tip, tip + QPointF(r * 0.25, -r * 0.02));
    p.drawLine(tip, tip + QPointF(r * 0.02, -r * 0.27));
    p.restore();
}

} // namespace cb
