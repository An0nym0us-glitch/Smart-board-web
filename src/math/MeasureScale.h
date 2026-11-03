#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace cb {

/// Length units understood by the scale system.
namespace units {
/// Metres per unit ("mm", "cm", "m", "km", "in", "ft", "yd", "mi"); 0 for unknown units.
double metresPer(const QString& unit);
bool isLengthUnit(const QString& unit);
/// Units offered in the interface, in display order.
QStringList lengthUnits();
/// Converts a value between two length units (returns the value unchanged for unknown units).
double convert(double value, const QString& from, const QString& to);
} // namespace units

/// Drawing scale of a lesson: "boardValue boardUnit on the board = realValue realUnit in reality",
/// e.g. 10 cm = 1 km. Board distances are logical document lengths (never screen pixels), so the
/// scale is independent of zoom, DPI and display size.
struct MeasureScale
{
    double boardValue = 1.0;
    QString boardUnit = QStringLiteral("cm");
    double realValue = 1.0;
    QString realUnit = QStringLiteral("cm");

    bool isValid() const;
    /// True when real lengths equal board lengths (1 cm = 1 cm, 10 mm = 1 cm ...).
    bool isIdentity() const;
    /// Real length per board length (dimensionless, both in metres).
    double factor() const;

    /// Converts a board length expressed in boardLengthUnit to the real unit, and back.
    double toReal(double boardLength, const QString& boardLengthUnit) const;
    double toBoard(double realLength, const QString& boardLengthUnit) const;

    /// "10 cm = 1 km"
    QString text() const;

    QJsonObject toJson() const;
    static MeasureScale fromJson(const QJsonObject& obj);
    bool operator==(const MeasureScale& o) const;
    bool operator!=(const MeasureScale& o) const { return !(*this == o); }
};

} // namespace cb
