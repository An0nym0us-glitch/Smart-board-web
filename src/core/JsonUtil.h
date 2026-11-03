#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QSizeF>
#include <QVector>

namespace cb::json {

inline QJsonValue fromColor(const QColor& c) { return c.name(QColor::HexArgb); }
inline QColor toColor(const QJsonValue& v, const QColor& fallback = Qt::white)
{
    const QColor c(v.toString());
    return c.isValid() ? c : fallback;
}

inline QJsonArray fromPoint(const QPointF& p) { return QJsonArray{p.x(), p.y()}; }
inline QPointF toPoint(const QJsonValue& v, const QPointF& fallback = QPointF())
{
    const QJsonArray a = v.toArray();
    if (a.size() < 2)
        return fallback;
    return QPointF(a.at(0).toDouble(), a.at(1).toDouble());
}

inline QJsonArray fromSize(const QSizeF& s) { return QJsonArray{s.width(), s.height()}; }
inline QSizeF toSize(const QJsonValue& v, const QSizeF& fallback = QSizeF())
{
    const QJsonArray a = v.toArray();
    if (a.size() < 2)
        return fallback;
    return QSizeF(a.at(0).toDouble(), a.at(1).toDouble());
}

/// Points are stored as a flat array [x0, y0, x1, y1, ...] to keep files compact.
inline QJsonArray fromPoints(const QVector<QPointF>& pts)
{
    QJsonArray a;
    for (const QPointF& p : pts) {
        a.append(p.x());
        a.append(p.y());
    }
    return a;
}

inline QVector<QPointF> toPoints(const QJsonValue& v)
{
    const QJsonArray a = v.toArray();
    QVector<QPointF> pts;
    pts.reserve(a.size() / 2);
    for (int i = 0; i + 1 < a.size(); i += 2)
        pts.push_back(QPointF(a.at(i).toDouble(), a.at(i + 1).toDouble()));
    return pts;
}

} // namespace cb::json
