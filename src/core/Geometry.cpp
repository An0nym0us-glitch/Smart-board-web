#include "core/Geometry.h"

#include <QString>

#include <algorithm>
#include <limits>

namespace cb::geom {

QPointF rotated(const QPointF& v, double degrees)
{
    const double r = degToRad(degrees);
    const double c = std::cos(r);
    const double s = std::sin(r);
    return QPointF(v.x() * c - v.y() * s, v.x() * s + v.y() * c);
}

QPointF closestPointOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF ab = b - a;
    const double len2 = dot(ab, ab);
    if (len2 <= 1e-12)
        return a;
    const double t = std::clamp(dot(p - a, ab) / len2, 0.0, 1.0);
    return a + ab * t;
}

double distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    return distance(p, closestPointOnSegment(p, a, b));
}

QPointF projectOntoLine(const QPointF& p, const QPointF& origin, const QPointF& dir)
{
    const QPointF d = normalized(dir);
    return origin + d * dot(p - origin, d);
}

double normalizeDegrees(double deg)
{
    double r = std::fmod(deg, 360.0);
    if (r < 0.0)
        r += 360.0;
    return r;
}

double angleDeg(const QPointF& v)
{
    return normalizeDegrees(radToDeg(std::atan2(v.y(), v.x())));
}

double angleDifference(double a, double b)
{
    double d = std::fmod(b - a, 360.0);
    if (d <= -180.0)
        d += 360.0;
    else if (d > 180.0)
        d -= 360.0;
    return d;
}

double snapAngle(double deg, double step, double tolerance, bool* snapped)
{
    const double nearest = std::round(deg / step) * step;
    const bool ok = std::abs(nearest - deg) <= tolerance;
    if (snapped)
        *snapped = ok;
    return ok ? nearest : deg;
}

bool segmentCircleInterval(const QPointF& a, const QPointF& b, const QPointF& center, double radius,
                           double& t0, double& t1)
{
    const QPointF d = b - a;
    const QPointF f = a - center;
    const double A = dot(d, d);
    const double r2 = radius * radius;
    if (A <= 1e-12) {
        // Degenerate segment: a single point.
        if (dot(f, f) <= r2) {
            t0 = 0.0;
            t1 = 1.0;
            return true;
        }
        return false;
    }
    const double B = 2.0 * dot(f, d);
    const double C = dot(f, f) - r2;
    const double disc = B * B - 4.0 * A * C;
    if (disc < 0.0)
        return false;
    const double sq = std::sqrt(disc);
    double s0 = (-B - sq) / (2.0 * A);
    double s1 = (-B + sq) / (2.0 * A);
    if (s1 < 0.0 || s0 > 1.0)
        return false;
    t0 = std::max(0.0, s0);
    t1 = std::min(1.0, s1);
    return t1 >= t0;
}

double polygonArea(const QVector<QPointF>& polygon)
{
    const int n = polygon.size();
    if (n < 3)
        return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const QPointF& p = polygon[i];
        const QPointF& q = polygon[(i + 1) % n];
        sum += p.x() * q.y() - q.x() * p.y();
    }
    return std::abs(sum) * 0.5;
}

double polylineLength(const QVector<QPointF>& points, bool closed)
{
    double total = 0.0;
    for (int i = 1; i < points.size(); ++i)
        total += distance(points[i - 1], points[i]);
    if (closed && points.size() > 2)
        total += distance(points.last(), points.first());
    return total;
}

namespace {
void rdp(const QVector<QPointF>& pts, int first, int last, double eps, QVector<bool>& keep)
{
    // Iterative stack to avoid deep recursion on long strokes.
    QVector<QPair<int, int>> stack;
    stack.push_back({first, last});
    while (!stack.isEmpty()) {
        const auto range = stack.takeLast();
        const int a = range.first;
        const int b = range.second;
        if (b <= a + 1)
            continue;
        double maxDist = -1.0;
        int index = -1;
        for (int i = a + 1; i < b; ++i) {
            const double d = distanceToSegment(pts[i], pts[a], pts[b]);
            if (d > maxDist) {
                maxDist = d;
                index = i;
            }
        }
        if (maxDist > eps && index > 0) {
            keep[index] = true;
            stack.push_back({a, index});
            stack.push_back({index, b});
        }
    }
}
} // namespace

QVector<int> simplifyIndices(const QVector<QPointF>& points, double epsilon)
{
    QVector<int> result;
    const int n = points.size();
    if (n <= 2) {
        for (int i = 0; i < n; ++i)
            result.push_back(i);
        return result;
    }
    QVector<bool> keep(n, false);
    keep[0] = true;
    keep[n - 1] = true;
    rdp(points, 0, n - 1, epsilon, keep);
    for (int i = 0; i < n; ++i)
        if (keep[i])
            result.push_back(i);
    return result;
}

QRectF boundingRect(const QVector<QPointF>& points)
{
    if (points.isEmpty())
        return {};
    double minX = std::numeric_limits<double>::max();
    double minY = minX;
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = maxX;
    for (const QPointF& p : points) {
        minX = std::min(minX, p.x());
        minY = std::min(minY, p.y());
        maxX = std::max(maxX, p.x());
        maxY = std::max(maxY, p.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

double niceNumber(double value, bool round)
{
    if (value <= 0.0)
        return 1.0;
    const double exponent = std::floor(std::log10(value));
    const double fraction = value / std::pow(10.0, exponent);
    double nice;
    if (round) {
        if (fraction < 1.5)
            nice = 1.0;
        else if (fraction < 3.0)
            nice = 2.0;
        else if (fraction < 7.0)
            nice = 5.0;
        else
            nice = 10.0;
    } else {
        if (fraction <= 1.0)
            nice = 1.0;
        else if (fraction <= 2.0)
            nice = 2.0;
        else if (fraction <= 5.0)
            nice = 5.0;
        else
            nice = 10.0;
    }
    return nice * std::pow(10.0, exponent);
}

QString formatNumber(double value, int maxDecimals)
{
    if (!std::isfinite(value))
        return QStringLiteral("undefined");
    if (std::abs(value) < 0.5 * std::pow(10.0, -maxDecimals))
        value = 0.0;
    QString s = QString::number(value, 'f', maxDecimals);
    if (s.contains(QLatin1Char('.'))) {
        while (s.endsWith(QLatin1Char('0')))
            s.chop(1);
        if (s.endsWith(QLatin1Char('.')))
            s.chop(1);
    }
    if (s == QLatin1String("-0"))
        s = QStringLiteral("0");
    // Use a proper minus sign for classroom readability.
    if (s.startsWith(QLatin1Char('-')))
        s.replace(0, 1, QChar(0x2212));
    return s;
}

} // namespace cb::geom
