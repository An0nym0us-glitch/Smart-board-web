#pragma once

#include "document/DocumentObject.h"

namespace cb {

enum class ConstructKind {
    Point,
    Segment,
    Line,
    Ray,
    Vector,
};

QString constructKindName(ConstructKind kind);
ConstructKind constructKindFromName(const QString& name);

/// Geometric construction element placed in the mathematical coordinate system: points (with
/// coordinates), segments (with length), infinite lines, rays and vectors (with components).
class GeometryObject final : public DocumentObject
{
public:
    static constexpr qreal kLineExtent = 2400.0; ///< how far lines / rays are drawn beyond their points

    GeometryObject();
    static std::unique_ptr<GeometryObject> create(ConstructKind kind, const QVector<QPointF>& pagePoints,
                                                  const QColor& color, bool showLabel);

    ConstructKind kind() const { return m_kind; }
    QVector<QPointF> pagePoints() const;
    bool showLabel() const { return m_showLabel; }
    void setShowLabel(bool on) { m_showLabel = on; }
    const QString& name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    /// Label text in the given coordinate system ("A (2, 3)", "|AB| = 4 cm", "v = ⟨3, 2⟩").
    QString labelText(const CoordinateSystem& cs) const;

    QRectF localBounds() const override;
    /// Room for the value label beside the figure (lengths in real units can be long).
    qreal outlineMargin() const override { return 200.0; }
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool canResize() const override { return false; }
    bool canRotate() const override { return false; }
    QVector<QPointF> controlPoints() const override { return m_points; }
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_color; }
    std::unique_ptr<DocumentObject> clone() const override;

    /// Draws a construction preview from page points.
    static void paintPreview(QPainter& painter, ConstructKind kind, const QVector<QPointF>& pagePoints,
                             const QColor& color, const CoordinateSystem& cs, bool showLabel);

protected:
    bool hitTestLocal(const QPointF& local, qreal tolerance) const override;
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void setControlPoint(int index, const QPointF& local) override;
    void translateContent(const QPointF& delta) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    GeometryObject(const GeometryObject&) = default;
    static void paintShape(QPainter& p, ConstructKind kind, const QVector<QPointF>& pts, const QColor& color,
                           const QString& label);
    static QString format(ConstructKind kind, const QVector<QPointF>& pagePts, const QString& name,
                          const CoordinateSystem& cs);
    /// End points of the drawn segment (lines/rays extended).
    static void drawnSegment(ConstructKind kind, const QVector<QPointF>& pts, QPointF& a, QPointF& b);

    ConstructKind m_kind = ConstructKind::Point;
    QVector<QPointF> m_points; // local
    QColor m_color = QColor(129, 212, 250);
    bool m_showLabel = true;
    QString m_name;
};

} // namespace cb
