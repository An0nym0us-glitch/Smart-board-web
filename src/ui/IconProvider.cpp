#include "ui/IconProvider.h"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace cb {

IconProvider::IconProvider()
    : m_cache(400)
{
}

bool IconProvider::exists(const QString& name) const
{
    return QFile::exists(QStringLiteral(":/icons/%1.svg").arg(name));
}

QPixmap IconProvider::pixmap(const QString& name, qreal size, const QColor& color, qreal dpr) const
{
    const int px = qMax(1, qRound(size * dpr));
    const QString key = QStringLiteral("%1|%2|%3").arg(name).arg(px).arg(color.rgba());
    if (QPixmap* cached = m_cache.object(key))
        return *cached;

    QImage image(px, px, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QSvgRenderer renderer(QStringLiteral(":/icons/%1.svg").arg(name));
    if (renderer.isValid()) {
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, px, px));
        // Tint: keep alpha of the artwork, replace colour.
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(image.rect(), color);
        p.end();
    }
    auto* pm = new QPixmap(QPixmap::fromImage(image));
    pm->setDevicePixelRatio(dpr);
    const QPixmap result = *pm;
    m_cache.insert(key, pm, 1);
    return result;
}

} // namespace cb
