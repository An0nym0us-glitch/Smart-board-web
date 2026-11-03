#include "canvas/ViewTransform.h"

#include <algorithm>

namespace cb {

void ViewTransform::setZoom(qreal zoom)
{
    m_zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
}

QRectF ViewTransform::pageToView(const QRectF& r) const
{
    return QRectF(pageToView(r.topLeft()), r.size() * m_zoom);
}

QRectF ViewTransform::viewToPage(const QRectF& r) const
{
    return QRectF(viewToPage(r.topLeft()), r.size() / m_zoom);
}

QTransform ViewTransform::toTransform() const
{
    return QTransform(m_zoom, 0, 0, m_zoom, m_offset.x(), m_offset.y());
}

void ViewTransform::zoomAt(const QPointF& viewPos, qreal factor)
{
    const QPointF pagePos = viewToPage(viewPos);
    setZoom(m_zoom * factor);
    m_offset = viewPos - pagePos * m_zoom;
}

void ViewTransform::fit(const QRectF& pageRect, const QRectF& viewRect, qreal margin)
{
    if (pageRect.isEmpty() || viewRect.isEmpty())
        return;
    const QRectF target = viewRect.adjusted(margin, margin, -margin, -margin);
    const qreal z = std::min(target.width() / pageRect.width(), target.height() / pageRect.height());
    setZoom(z);
    m_offset = target.center() - pageRect.center() * m_zoom;
}

} // namespace cb
