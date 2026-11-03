#pragma once

#include "document/DocumentObject.h"

namespace cb {

enum class MeasureKind {
    Distance, ///< two points
    Angle,    ///< arm end, vertex, arm end
    Slope,    ///< two points
    Area,     ///< polygon (>= 3 points)
};

QString measureKindName(MeasureKind kind);
MeasureKind measureKindFromName(const QString& name);

/// A live measurement drawn on the page. The value is recomputed from the current point positions
/// in the applicable coordinate system (page units, or graph units inside a graph).
class MeasurementObject final : public DocumentObject
{
public:
    MeasurementObject();
    static std::unique_ptr<MeasurementObject> create(MeasureKind kind, const QVector<QPointF>& pagePoints,
                                                     const QColor& color);

    MeasureKind kind() const { return m_kind; }
    QVector<QPointF> pagePoints() const;

    /// Formatted value (e.g. "5.2 cm", "45°", "m = 0.5", "A = 12 cm²").
    QString valueText(const CoordinateSystem& cs) const;

    QRectF localBounds() const override;
    /// Room for the value label (areas with perimeter and real units can be long).
    qreal outlineMargin() const override { return 170.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool canResize() const override { return false; }
    bool canRotate() const override { return false; }
    QVector<QPointF> controlPoints() const override { return m_points; }
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_color; }
    std::unique_ptr<DocumentObject> clone() const override;

    /// Interior angle at pts[1] in degrees (0..180), -1 if undefined.
    static double angleDegrees(const QVector<QPointF>& pagePts, const CoordinateSystem& cs);
    /// Perimeter of a closed polygon in math units.
    static double perimeter(const QVector<QPointF>& pagePts, const CoordinateSystem& cs);
    static QString format(MeasureKind kind, const QVector<QPointF>& pagePts, const CoordinateSystem& cs);

    /// Renders a measurement preview from page points (used by the measure tool).
    static void paintPreview(QPainter& painter, MeasureKind kind, const QVector<QPointF>& pagePoints,
                             const QColor& color, const CoordinateSystem& cs, qreal zoom);

protected:
    bool hitTestLocal(const QPointF& local, qreal tolerance) const override;
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void setControlPoint(int index, const QPointF& local) override;
    void translateContent(const QPointF& delta) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    MeasurementObject(const MeasurementObject&) = default;
    static void paintShape(QPainter& p, MeasureKind kind, const QVector<QPointF>& pts, const QColor& color,
                           const QString& label, qreal rotation, qreal zoom);

    MeasureKind m_kind = MeasureKind::Distance;
    QVector<QPointF> m_points; // local
    QColor m_color = QColor(255, 213, 79);
};

} // namespace cb
