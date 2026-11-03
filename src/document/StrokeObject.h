#pragma once

#include "document/DocumentObject.h"

#include <QPainterPath>

namespace cb {

enum class StrokeStyle {
    Pen,
    Highlighter,
    Dashed,
    Dotted,
};

QString strokeStyleName(StrokeStyle style);
StrokeStyle strokeStyleFromName(const QString& name);

struct StrokePoint
{
    QPointF pos;
    float pressure = 1.0f;
};

/// Visual parameters of ink.
struct InkStyle
{
    QColor color = Qt::white;
    qreal width = 4.0;
    StrokeStyle style = StrokeStyle::Pen;
    bool pressure = false;
};

/// Freehand ink.
class StrokeObject final : public DocumentObject
{
public:
    StrokeObject();

    /// Creates a stroke from page-space points; the local frame is centred automatically.
    static std::unique_ptr<StrokeObject> fromPagePoints(const QVector<StrokePoint>& pagePoints, const InkStyle& ink);

    const QVector<StrokePoint>& points() const { return m_points; }
    QVector<QPointF> pagePoints() const;

    const InkStyle& ink() const { return m_ink; }
    void setInk(const InkStyle& ink);

    QRectF localBounds() const override;
    qreal outlineMargin() const override;
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool isInsidePolygon(const QPolygonF& pagePolygon) const override;
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_ink.color; }
    std::unique_ptr<DocumentObject> clone() const override;

    /// Erases the part of the stroke covered by a circle (page coordinates). Returns the remaining
    /// fragments as new strokes in page coordinates; sets *touched to whether anything was erased.
    std::vector<std::unique_ptr<StrokeObject>> eraseCircle(const QPointF& pageCenter, qreal radius,
                                                           bool* touched) const;

    /// Renders arbitrary points with an ink style (used for live previews).
    static void paintPoints(QPainter& painter, const QVector<StrokePoint>& points, const InkStyle& ink);

    /// Effective colour used when painting (highlighters are translucent).
    static QColor effectiveColor(const InkStyle& ink);

protected:
    bool hitTestLocal(const QPointF& local, qreal tolerance) const override;
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void translateContent(const QPointF& delta) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    StrokeObject(const StrokeObject& other) = default;
    void invalidateCache();
    static QPainterPath centerlinePath(const QVector<StrokePoint>& points);
    static QPainterPath pressureOutline(const QVector<StrokePoint>& points, qreal width);

    QVector<StrokePoint> m_points;
    InkStyle m_ink;
    mutable QPainterPath m_pathCache;
    mutable bool m_pathValid = false;
    mutable QRectF m_boundsCache;
    mutable bool m_localBoundsValid = false;
};

} // namespace cb
