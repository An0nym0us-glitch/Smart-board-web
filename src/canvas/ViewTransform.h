#pragma once

#include <QPointF>
#include <QRectF>
#include <QTransform>

namespace cb {

/// Page <-> view mapping: view = page * zoom + offset.
class ViewTransform
{
public:
    static constexpr qreal kMinZoom = 0.05;
    static constexpr qreal kMaxZoom = 12.0;

    qreal zoom() const { return m_zoom; }
    QPointF offset() const { return m_offset; }

    void setZoom(qreal zoom);
    void setOffset(const QPointF& offset) { m_offset = offset; }

    QPointF pageToView(const QPointF& p) const { return p * m_zoom + m_offset; }
    QPointF viewToPage(const QPointF& v) const { return (v - m_offset) / m_zoom; }
    QRectF pageToView(const QRectF& r) const;
    QRectF viewToPage(const QRectF& r) const;
    qreal viewToPage(qreal length) const { return length / m_zoom; }

    QTransform toTransform() const;

    /// Zooms by factor keeping the page point under viewPos fixed.
    void zoomAt(const QPointF& viewPos, qreal factor);
    void panBy(const QPointF& viewDelta) { m_offset += viewDelta; }

    /// Fits a page rect into a view rect with a margin.
    void fit(const QRectF& pageRect, const QRectF& viewRect, qreal margin);

    bool operator==(const ViewTransform& o) const { return m_zoom == o.m_zoom && m_offset == o.m_offset; }
    bool operator!=(const ViewTransform& o) const { return !(*this == o); }

private:
    qreal m_zoom = 1.0;
    QPointF m_offset;
};

} // namespace cb
