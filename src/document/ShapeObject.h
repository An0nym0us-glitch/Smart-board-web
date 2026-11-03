#pragma once

#include "document/DocumentObject.h"

#include <QPainterPath>

namespace cb {

enum class ShapeKind {
    Line,
    Arrow,
    DoubleArrow,
    Rectangle,
    RoundedRect,
    Ellipse,
    Circle,
    Triangle,
    RightTriangle,
    Diamond,
    Parallelogram,
    Hexagon,
    RegularPolygon,
    FreePolygon,
};

QString shapeKindName(ShapeKind kind);
ShapeKind shapeKindFromName(const QString& name);
QString shapeKindLabel(ShapeKind kind);
bool isLineShape(ShapeKind kind);

struct ShapeStyle
{
    QColor stroke = Qt::white;
    qreal width = 4.0;
    QColor fill = Qt::transparent;
    bool dashed = false;
};

/// Vector shape. Box shapes are resizable/rotatable; line shapes and free polygons are edited
/// through their control points.
class ShapeObject final : public DocumentObject
{
public:
    ShapeObject();

    static std::unique_ptr<ShapeObject> createBox(ShapeKind kind, const QRectF& pageRect, const ShapeStyle& style,
                                                  int sides = 5);
    static std::unique_ptr<ShapeObject> createLine(ShapeKind kind, const QPointF& a, const QPointF& b,
                                                   const ShapeStyle& style);
    static std::unique_ptr<ShapeObject> createPolygon(const QVector<QPointF>& pagePoints, const ShapeStyle& style);

    /// Outline of a box shape of the given size centred on the origin (also used for icons).
    static QPainterPath outlineFor(ShapeKind kind, const QSizeF& size, int sides = 5);

    ShapeKind kind() const { return m_kind; }
    const ShapeStyle& style() const { return m_style; }
    void setStyle(const ShapeStyle& style) { m_style = style; }
    int sides() const { return m_sides; }
    /// Size of a box shape in local units (unrotated).
    QSizeF boxSize() const { return m_size; }

    QRectF localBounds() const override;
    qreal outlineMargin() const override;
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool canResize() const override { return !isLineShape(m_kind); }
    bool canRotate() const override { return !isLineShape(m_kind); }
    bool keepAspectRatio() const override { return m_kind == ShapeKind::Circle; }
    QVector<QPointF> controlPoints() const override;
    bool enclosesPoint(const QPointF& pagePos) const override;
    bool setColor(const QColor& color) override;
    QColor color() const override { return m_style.stroke; }
    std::unique_ptr<DocumentObject> clone() const override;

    /// The shape as freehand ink in page coordinates, so the area eraser can rub out part of it:
    /// its outline (or line) as straight strokes with the same colour, width and dash style,
    /// plus filled arrowheads as small filled polygons. A fill is not carried over.
    std::vector<std::unique_ptr<DocumentObject>> toInk() const;

    /// Paints a line/arrow between two points (used for previews and vectors).
    static void paintArrow(QPainter& painter, const QPointF& a, const QPointF& b, const QPen& pen, bool startHead,
                           bool endHead);
    /// Geometry of paintArrow: the shaft end points and the arrowhead triangles.
    static void arrowGeometry(const QPointF& a, const QPointF& b, qreal penWidth, bool startHead, bool endHead,
                              QLineF* shaft, QVector<QPolygonF>* heads);

protected:
    bool hitTestLocal(const QPointF& local, qreal tolerance) const override;
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void setControlPoint(int index, const QPointF& local) override;
    void translateContent(const QPointF& delta) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    ShapeObject(const ShapeObject&) = default;
    QPainterPath outline() const;

    ShapeKind m_kind = ShapeKind::Rectangle;
    ShapeStyle m_style;
    QSizeF m_size{100, 100};
    QPointF m_p1{-50, 0};
    QPointF m_p2{50, 0};
    QVector<QPointF> m_points;
    int m_sides = 5;
};

} // namespace cb
