#pragma once

#include "core/Id.h"

#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QVector>

namespace cb {

class CoordinateSystem;
class Document;
class DocumentObject;
class Page;

/// Exact numeric editing of geometry. All functions work on page points and a coordinate system,
/// so values are mathematical (document units, optionally through the lesson's drawing scale) and
/// never depend on zoom, DPI or the screen. Directions are measured counter-clockwise from the
/// positive x axis, as in mathematics (the page y axis points down, the math y axis up).
namespace precision {

// ---- Two-point figures: segment, vector, line shape, distance / slope measurement ----------

/// Length between pts[0] and pts[1] in math units.
double length(const QVector<QPointF>& pts, const CoordinateSystem& cs);
/// Direction of pts[1] seen from pts[0] in degrees [0, 360).
double direction(const QVector<QPointF>& pts, const CoordinateSystem& cs);
/// Moves pts[1] (pts[0] stays) so the figure has the given length / direction / both.
QVector<QPointF> withLength(const QVector<QPointF>& pts, double mathLength, const CoordinateSystem& cs);
QVector<QPointF> withDirection(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs);
QVector<QPointF> withPolar(const QVector<QPointF>& pts, double mathLength, double degrees, const CoordinateSystem& cs);

// ---- Angle (arm point, vertex, arm point) ---------------------------------------------------

/// Interior angle at pts[1] in degrees (0..180), -1 if undefined.
double angle(const QVector<QPointF>& pts, const CoordinateSystem& cs);
/// Rotates the second arm (pts[2]) about the vertex so the angle is exactly degrees (0..360),
/// keeping the first arm, the arm lengths and the side on which the angle opens.
QVector<QPointF> withAngle(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs);

// ---- Slope (two points) -----------------------------------------------------------------------

struct SlopeValues
{
    double rise = 0.0;  ///< Δy in math units
    double run = 0.0;   ///< Δx in math units
    double slope = 0.0; ///< rise / run (undefined if vertical)
    bool vertical = false;
    double angle = 0.0; ///< inclination in degrees (-90..90]
};
SlopeValues slopeValues(const QVector<QPointF>& pts, const CoordinateSystem& cs);
/// pts[1] = pts[0] + (run, rise) in math coordinates.
QVector<QPointF> withRiseRun(const QVector<QPointF>& pts, double rise, double run, const CoordinateSystem& cs);
/// Keeps the run and sets rise = slope × run.
QVector<QPointF> withSlope(const QVector<QPointF>& pts, double slope, const CoordinateSystem& cs);
/// Keeps the length and the horizontal direction, sets the inclination (-90..90 degrees).
QVector<QPointF> withInclination(const QVector<QPointF>& pts, double degrees, const CoordinateSystem& cs);

// ---- Shapes -------------------------------------------------------------------------------------

struct ShapeMetrics
{
    double width = 0.0;     ///< math units
    double height = 0.0;
    double area = 0.0;      ///< square math units
    double perimeter = 0.0; ///< math units
};
/// Width, height, area and perimeter of a box shape (rectangle, circle, triangle ...), a free
/// polygon or an area measurement.
bool shapeMetrics(const DocumentObject& object, const CoordinateSystem& cs, ShapeMetrics* out);

// ---- Applying edits ---------------------------------------------------------------------------

/// Page points that define the object for precision editing (empty if not supported).
QVector<QPointF> definingPoints(const DocumentObject& object);
/// Moves the object's defining points to newPagePoints as one undoable command.
bool applyPoints(Document& doc, Page& page, const ObjectId& id, const QVector<QPointF>& newPagePoints,
                 const QString& text);
/// Resizes a box shape to an exact size in math units (keeps centre and rotation), undoable.
bool applyShapeSize(Document& doc, Page& page, const ObjectId& id, double mathWidth, double mathHeight,
                    const CoordinateSystem& cs, const QString& text);

/// The coordinate system that applies to an object on a page (graph units inside graphs,
/// otherwise the lesson's system with its scale).
CoordinateSystem coordinateSystemFor(const Document& doc, const Page* page, const DocumentObject& object);

} // namespace precision
} // namespace cb
