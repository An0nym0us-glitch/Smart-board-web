#pragma once

#include <QLineF>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QVector>

#include <cmath>

namespace cb::geom {

constexpr double kPi = 3.14159265358979323846;

inline double degToRad(double deg) { return deg * kPi / 180.0; }
inline double radToDeg(double rad) { return rad * 180.0 / kPi; }

inline double length(const QPointF& v) { return std::hypot(v.x(), v.y()); }
inline double distance(const QPointF& a, const QPointF& b) { return length(b - a); }
inline double dot(const QPointF& a, const QPointF& b) { return a.x() * b.x() + a.y() * b.y(); }
inline double cross(const QPointF& a, const QPointF& b) { return a.x() * b.y() - a.y() * b.x(); }
inline QPointF lerp(const QPointF& a, const QPointF& b, double t) { return a + (b - a) * t; }
inline QPointF midpoint(const QPointF& a, const QPointF& b) { return (a + b) * 0.5; }

inline QPointF normalized(const QPointF& v)
{
    const double len = length(v);
    return len > 1e-12 ? v / len : QPointF(0.0, 0.0);
}

inline QPointF perpendicular(const QPointF& v) { return QPointF(-v.y(), v.x()); }

inline QRectF inflated(const QRectF& r, qreal margin) { return r.adjusted(-margin, -margin, margin, margin); }

/// Rotates v by the given angle (degrees, clockwise in screen coordinates where y grows downwards).
QPointF rotated(const QPointF& v, double degrees);

/// Distance from p to the closed segment [a, b].
double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b);
QPointF closestPointOnSegment(const QPointF& p, const QPointF& a, const QPointF& b);

/// Orthogonal projection of p onto the infinite line through origin with direction dir.
QPointF projectOntoLine(const QPointF& p, const QPointF& origin, const QPointF& dir);

/// Angle of vector v in degrees in [0, 360), measured in screen coordinates (y down).
double angleDeg(const QPointF& v);

/// Normalises an angle to [0, 360).
double normalizeDegrees(double deg);

/// Smallest signed difference b - a in degrees, in (-180, 180].
double angleDifference(double a, double b);

/// Snaps deg to a multiple of step if within tolerance.
double snapAngle(double deg, double step, double tolerance, bool* snapped = nullptr);

/// Computes the parameter interval [t0, t1] (clamped to [0, 1]) of segment a->b lying inside the circle.
/// Returns false if the segment does not intersect the circle.
bool segmentCircleInterval(const QPointF& a, const QPointF& b, const QPointF& center, double radius,
                           double& t0, double& t1);

/// Absolute polygon area (shoelace formula).
double polygonArea(const QVector<QPointF>& polygon);

/// Polygon perimeter; closed controls whether the closing edge is included.
double polylineLength(const QVector<QPointF>& points, bool closed = false);

/// Ramer-Douglas-Peucker simplification. Returns the indices of retained points.
QVector<int> simplifyIndices(const QVector<QPointF>& points, double epsilon);

/// Bounding rect of a point list.
QRectF boundingRect(const QVector<QPointF>& points);

/// Returns a "nice" number (1, 2, 5 x 10^n) close to value, used for axis ticks.
double niceNumber(double value, bool round);

/// Formats a number for display with at most the given decimals, trimming trailing zeros.
QString formatNumber(double value, int maxDecimals = 2);

} // namespace cb::geom
