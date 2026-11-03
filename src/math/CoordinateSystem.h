#pragma once

#include "math/MeasureScale.h"

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QVector>

namespace cb {

/// Maps between page coordinates (pixels, y down) and mathematical coordinates (units, y up).
///
/// A single coordinate system type is shared by the page-wide coordinate plane, geometry
/// measurements and function graphs so that all mathematical tools agree on units.
class CoordinateSystem
{
public:
    CoordinateSystem();

    /// Builds the page-global system: origin at originPx, pxPerUnit pixels per unit, y axis up.
    static CoordinateSystem global(const QPointF& originPx, double pxPerUnit, const QString& unitLabel);

    /// Builds a system from an arbitrary page->math transform.
    static CoordinateSystem fromTransform(const QTransform& pageToMath, const QString& unitLabel);

    QPointF toMath(const QPointF& pagePoint) const { return m_pageToMath.map(pagePoint); }
    QPointF toPage(const QPointF& mathPoint) const { return m_mathToPage.map(mathPoint); }

    /// Length in math units of a page-space vector starting at from.
    double mathDistance(const QPointF& pageA, const QPointF& pageB) const;

    /// Slope dy/dx in math coordinates; returns false for vertical segments.
    bool mathSlope(const QPointF& pageA, const QPointF& pageB, double* slope) const;

    /// Signed angle of the math-space vector in degrees (counter-clockwise from +x).
    double mathAngle(const QPointF& pageA, const QPointF& pageB) const;

    /// Area in square math units of a page-space polygon.
    double mathArea(const QVector<QPointF>& pagePolygon) const;

    const QString& unitLabel() const { return m_unitLabel; }
    const QTransform& pageToMath() const { return m_pageToMath; }
    const QTransform& mathToPage() const { return m_mathToPage; }

    /// Drawing scale ("10 cm = 1 km"). Only applies to length units (not to unitless graph axes).
    const MeasureScale& scale() const { return m_scale; }
    void setScale(const MeasureScale& scale) { m_scale = scale.isValid() ? scale : MeasureScale(); }
    /// True if lengths are shown in real-world units different from the board unit.
    bool usesScale() const { return units::isLengthUnit(m_unitLabel) && !m_scale.isIdentity(); }
    /// Unit of displayed (real-world) lengths: the scale's real unit or the board unit.
    QString realUnitLabel() const { return usesScale() ? m_scale.realUnit : m_unitLabel; }
    /// Converts a length in math units (board units, e.g. cm) to real units and back.
    double toReal(double mathLength) const;
    double fromReal(double realLength) const;
    /// Converts an area in square math units to square real units.
    double toRealArea(double mathArea) const;
    /// "7 cm" or, with a scale, "0.7 km".
    QString formatLength(double mathLength, int decimals = 2) const;
    /// "12 cm²" or, with a scale, "1.2 km²".
    QString formatArea(double mathArea, int decimals = 2) const;

    /// The same system with its origin moved by pageDelta (used to centre the global system on
    /// pages whose size differs from the default).
    CoordinateSystem translated(const QPointF& pageDelta) const;

    /// Pixels per unit along the x axis (for ruler/grid rendering).
    double pxPerUnit() const;
    QPointF originPx() const { return toPage(QPointF(0, 0)); }

    QJsonObject toJson() const;
    static CoordinateSystem fromJson(const QJsonObject& obj);

private:
    QTransform m_pageToMath;
    QTransform m_mathToPage;
    QString m_unitLabel;
    MeasureScale m_scale;
};

} // namespace cb
