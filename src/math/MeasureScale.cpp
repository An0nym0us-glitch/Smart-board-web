#include "math/MeasureScale.h"

#include "core/Geometry.h"

#include <cmath>

namespace cb {

namespace units {

double metresPer(const QString& unit)
{
    const QString u = unit.trimmed().toLower();
    if (u == QLatin1String("mm"))
        return 0.001;
    if (u == QLatin1String("cm"))
        return 0.01;
    if (u == QLatin1String("m"))
        return 1.0;
    if (u == QLatin1String("km"))
        return 1000.0;
    if (u == QLatin1String("in"))
        return 0.0254;
    if (u == QLatin1String("ft"))
        return 0.3048;
    if (u == QLatin1String("yd"))
        return 0.9144;
    if (u == QLatin1String("mi"))
        return 1609.344;
    return 0.0;
}

bool isLengthUnit(const QString& unit)
{
    return metresPer(unit) > 0.0;
}

QStringList lengthUnits()
{
    return {QStringLiteral("mm"), QStringLiteral("cm"), QStringLiteral("m"),  QStringLiteral("km"),
            QStringLiteral("in"), QStringLiteral("ft"), QStringLiteral("yd"), QStringLiteral("mi")};
}

double convert(double value, const QString& from, const QString& to)
{
    const double a = metresPer(from);
    const double b = metresPer(to);
    if (a <= 0.0 || b <= 0.0)
        return value;
    return value * a / b;
}

} // namespace units

bool MeasureScale::isValid() const
{
    return boardValue > 0.0 && realValue > 0.0 && std::isfinite(boardValue) && std::isfinite(realValue)
        && units::isLengthUnit(boardUnit) && units::isLengthUnit(realUnit);
}

double MeasureScale::factor() const
{
    if (!isValid())
        return 1.0;
    return (realValue * units::metresPer(realUnit)) / (boardValue * units::metresPer(boardUnit));
}

bool MeasureScale::isIdentity() const
{
    return !isValid() || (std::abs(factor() - 1.0) < 1e-12 && boardUnit == realUnit);
}

double MeasureScale::toReal(double boardLength, const QString& boardLengthUnit) const
{
    if (!isValid())
        return boardLength;
    const double metres = boardLength * units::metresPer(boardLengthUnit) * factor();
    return metres / units::metresPer(realUnit);
}

double MeasureScale::toBoard(double realLength, const QString& boardLengthUnit) const
{
    if (!isValid() || units::metresPer(boardLengthUnit) <= 0.0)
        return realLength;
    const double metres = realLength * units::metresPer(realUnit) / factor();
    return metres / units::metresPer(boardLengthUnit);
}

QString MeasureScale::text() const
{
    return QStringLiteral("%1 %2 = %3 %4")
        .arg(geom::formatNumber(boardValue, 4), boardUnit, geom::formatNumber(realValue, 4), realUnit);
}

QJsonObject MeasureScale::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("board"), boardValue);
    o.insert(QStringLiteral("boardUnit"), boardUnit);
    o.insert(QStringLiteral("real"), realValue);
    o.insert(QStringLiteral("realUnit"), realUnit);
    return o;
}

MeasureScale MeasureScale::fromJson(const QJsonObject& obj)
{
    MeasureScale s;
    s.boardValue = obj.value(QStringLiteral("board")).toDouble(1.0);
    s.boardUnit = obj.value(QStringLiteral("boardUnit")).toString(QStringLiteral("cm"));
    s.realValue = obj.value(QStringLiteral("real")).toDouble(1.0);
    s.realUnit = obj.value(QStringLiteral("realUnit")).toString(QStringLiteral("cm"));
    if (!s.isValid())
        return MeasureScale();
    return s;
}

bool MeasureScale::operator==(const MeasureScale& o) const
{
    return boardValue == o.boardValue && boardUnit == o.boardUnit && realValue == o.realValue
        && realUnit == o.realUnit;
}

} // namespace cb
