#include "document/PageSize.h"

#include "core/Geometry.h"

#include <QCoreApplication>

#include <algorithm>
#include <cmath>

namespace cb::pagesize {

namespace {
QString tr(const char* s) { return QCoreApplication::translate("PageSize", s); }

bool near(const QSizeF& a, const QSizeF& b)
{
    return std::abs(a.width() - b.width()) < 0.5 && std::abs(a.height() - b.height()) < 0.5;
}
} // namespace

const QVector<Info>& presets()
{
    static const QVector<Info> list = {
        {Preset::Board16x9, QStringLiteral("board16x9"), tr("Classroom board 16:9"), QSizeF(48.0, 27.0)},
        {Preset::Board4x3, QStringLiteral("board4x3"), tr("4:3"), QSizeF(36.0, 27.0)},
        {Preset::A4, QStringLiteral("a4"), tr("A4"), QSizeF(29.7, 21.0)},
        {Preset::A3, QStringLiteral("a3"), tr("A3"), QSizeF(42.0, 29.7)},
    };
    return list;
}

QSizeF fromCentimetres(const QSizeF& cm)
{
    const qreal w = std::clamp(cm.width(), 5.0, 500.0);
    const qreal h = std::clamp(cm.height(), 5.0, 500.0);
    return QSizeF(w * kUnitsPerCm, h * kUnitsPerCm);
}

QSizeF toCentimetres(const QSizeF& units)
{
    return units / kUnitsPerCm;
}

QSizeF sizeFor(Preset preset, Orientation orientation)
{
    for (const Info& info : presets()) {
        if (info.preset != preset)
            continue;
        QSizeF cm = info.landscapeCm;
        if (orientation == Orientation::Portrait)
            cm.transpose();
        return QSizeF(cm.width() * kUnitsPerCm, cm.height() * kUnitsPerCm);
    }
    return QSizeF(48.0 * kUnitsPerCm, 27.0 * kUnitsPerCm);
}

Preset presetOf(const QSizeF& size)
{
    for (const Info& info : presets()) {
        if (near(size, sizeFor(info.preset, Orientation::Landscape)) || near(size, sizeFor(info.preset, Orientation::Portrait)))
            return info.preset;
    }
    return Preset::Custom;
}

Orientation orientationOf(const QSizeF& size)
{
    return size.height() > size.width() ? Orientation::Portrait : Orientation::Landscape;
}

QString describe(const QSizeF& size)
{
    const Preset preset = presetOf(size);
    const bool portrait = orientationOf(size) == Orientation::Portrait;
    for (const Info& info : presets()) {
        if (info.preset != preset)
            continue;
        if (preset == Preset::A4 || preset == Preset::A3)
            return portrait ? tr("%1 portrait").arg(info.label) : tr("%1 landscape").arg(info.label);
        return portrait ? tr("%1 (portrait)").arg(info.label) : info.label;
    }
    const QSizeF cm = toCentimetres(size);
    return tr("Custom %1 × %2 cm").arg(geom::formatNumber(cm.width(), 1), geom::formatNumber(cm.height(), 1));
}

QSizeF forAspect(qreal width, qreal height)
{
    const QSizeF board = sizeFor(Preset::Board16x9, Orientation::Landscape);
    if (width <= 0.0 || height <= 0.0)
        return board;
    const qreal longSide = board.width();
    if (width >= height)
        return QSizeF(longSide, longSide * height / width);
    return QSizeF(longSide * width / height, longSide);
}

} // namespace cb::pagesize
