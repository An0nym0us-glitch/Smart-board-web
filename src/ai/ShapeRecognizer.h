#pragma once

#include "document/DocumentObject.h"
#include "document/StrokeObject.h"

#include <QPointF>
#include <QVector>

namespace cb {

/// Offline, deterministic recogniser that turns hand-drawn ink into clean shapes
/// (line, circle, ellipse, triangle, rectangle, quadrilateral, pentagon, hexagon).
///
/// It uses geometric fitting only (no trained model), so results are predictable in class.
class ShapeRecognizer
{
public:
    struct Result
    {
        enum class Kind { None, Line, Circle, Ellipse, Rectangle, Polygon };
        Kind kind = Kind::None;
        QVector<QPointF> points; ///< line end points / polygon vertices (page)
        QRectF box;              ///< for circles, ellipses and axis aligned rectangles
        double confidence = 0.0;
    };

    /// Classifies page-space points. pixel is the page length of one screen pixel.
    Result classify(const QVector<QPointF>& points, qreal pixel) const;

    /// Returns a shape object or nullptr when the ink does not look like a clean shape.
    ObjectPtr recognize(const QVector<QPointF>& points, const InkStyle& ink, qreal pixel) const;
};

} // namespace cb
