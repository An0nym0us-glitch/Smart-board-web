#include "math/CoordinateSystem.h"

#include "core/Geometry.h"
#include "core/JsonUtil.h"

#include <cmath>

namespace cb {

CoordinateSystem::CoordinateSystem()
    : m_unitLabel(QStringLiteral("cm"))
{
    m_pageToMath.scale(1.0 / 40.0, -1.0 / 40.0);
    m_mathToPage = m_pageToMath.inverted();
}

CoordinateSystem CoordinateSystem::global(const QPointF& originPx, double pxPerUnit, const QString& unitLabel)
{
    if (pxPerUnit <= 0.0)
        pxPerUnit = 40.0;
    QTransform t;
    t.scale(1.0 / pxPerUnit, -1.0 / pxPerUnit);
    t.translate(-originPx.x(), -originPx.y());
    return fromTransform(t, unitLabel);
}

CoordinateSystem CoordinateSystem::fromTransform(const QTransform& pageToMath, const QString& unitLabel)
{
    CoordinateSystem cs;
    cs.m_pageToMath = pageToMath;
    bool invertible = false;
    cs.m_mathToPage = pageToMath.inverted(&invertible);
    if (!invertible)
        cs.m_mathToPage = QTransform();
    cs.m_unitLabel = unitLabel;
    return cs;
}

double CoordinateSystem::mathDistance(const QPointF& pageA, const QPointF& pageB) const
{
    return geom::distance(toMath(pageA), toMath(pageB));
}

bool CoordinateSystem::mathSlope(const QPointF& pageA, const QPointF& pageB, double* slope) const
{
    const QPointF a = toMath(pageA);
    const QPointF b = toMath(pageB);
    const double dx = b.x() - a.x();
    if (std::abs(dx) < 1e-9)
        return false;
    if (slope)
        *slope = (b.y() - a.y()) / dx;
    return true;
}

double CoordinateSystem::mathAngle(const QPointF& pageA, const QPointF& pageB) const
{
    const QPointF d = toMath(pageB) - toMath(pageA);
    return geom::normalizeDegrees(geom::radToDeg(std::atan2(d.y(), d.x())));
}

double CoordinateSystem::mathArea(const QVector<QPointF>& pagePolygon) const
{
    QVector<QPointF> m;
    m.reserve(pagePolygon.size());
    for (const QPointF& p : pagePolygon)
        m.push_back(toMath(p));
    return geom::polygonArea(m);
}

double CoordinateSystem::toReal(double mathLength) const
{
    return usesScale() ? m_scale.toReal(mathLength, m_unitLabel) : mathLength;
}

double CoordinateSystem::fromReal(double realLength) const
{
    return usesScale() ? m_scale.toBoard(realLength, m_unitLabel) : realLength;
}

double CoordinateSystem::toRealArea(double mathArea) const
{
    const double f = toReal(1.0);
    return mathArea * f * f;
}

QString CoordinateSystem::formatLength(double mathLength, int decimals) const
{
    const QString unit = realUnitLabel();
    const QString v = geom::formatNumber(toReal(mathLength), decimals);
    return unit.isEmpty() ? v : v + QLatin1Char(' ') + unit;
}

QString CoordinateSystem::formatArea(double mathArea, int decimals) const
{
    const QString unit = realUnitLabel();
    const QString v = geom::formatNumber(toRealArea(mathArea), decimals);
    return unit.isEmpty() ? v : v + QLatin1Char(' ') + unit + QStringLiteral("²");
}

CoordinateSystem CoordinateSystem::translated(const QPointF& pageDelta) const
{
    if (pageDelta.isNull())
        return *this;
    CoordinateSystem cs = fromTransform(QTransform::fromTranslate(-pageDelta.x(), -pageDelta.y()) * m_pageToMath, m_unitLabel);
    cs.m_scale = m_scale;
    return cs;
}

double CoordinateSystem::pxPerUnit() const
{
    const QPointF a = toPage(QPointF(0, 0));
    const QPointF b = toPage(QPointF(1, 0));
    return geom::distance(a, b);
}

QJsonObject CoordinateSystem::toJson() const
{
    const QTransform& t = m_pageToMath;
    QJsonObject o;
    o.insert(QStringLiteral("m"), QJsonArray{t.m11(), t.m12(), t.m21(), t.m22(), t.dx(), t.dy()});
    o.insert(QStringLiteral("unit"), m_unitLabel);
    if (!m_scale.isIdentity())
        o.insert(QStringLiteral("scale"), m_scale.toJson());
    return o;
}

CoordinateSystem CoordinateSystem::fromJson(const QJsonObject& obj)
{
    const QJsonArray m = obj.value(QStringLiteral("m")).toArray();
    if (m.size() != 6)
        return CoordinateSystem();
    QTransform t(m.at(0).toDouble(), m.at(1).toDouble(), m.at(2).toDouble(), m.at(3).toDouble(),
                 m.at(4).toDouble(), m.at(5).toDouble());
    CoordinateSystem cs = fromTransform(t, obj.value(QStringLiteral("unit")).toString(QStringLiteral("cm")));
    if (obj.contains(QStringLiteral("scale")))
        cs.setScale(MeasureScale::fromJson(obj.value(QStringLiteral("scale")).toObject()));
    return cs;
}

} // namespace cb
