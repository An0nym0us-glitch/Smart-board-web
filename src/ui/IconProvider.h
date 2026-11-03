#pragma once

#include <QCache>
#include <QColor>
#include <QPixmap>
#include <QString>

namespace cb {

/// Renders monochrome SVG icons from resources/icons, tinted to theme colours, at the correct
/// device pixel ratio. Rendered pixmaps are cached (bounded).
class IconProvider
{
public:
    IconProvider();

    /// Returns a tinted icon. size is in device independent pixels.
    QPixmap pixmap(const QString& name, qreal size, const QColor& color, qreal devicePixelRatio) const;

    bool exists(const QString& name) const;

private:
    mutable QCache<QString, QPixmap> m_cache;
};

} // namespace cb
