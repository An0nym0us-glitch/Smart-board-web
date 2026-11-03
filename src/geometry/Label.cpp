#include "geometry/Label.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>

#include <cmath>

namespace cb {

namespace {
QFont labelFont(qreal pixelSize)
{
    QFont f;
    f.setPixelSize(static_cast<int>(pixelSize));
    f.setBold(true);
    return f;
}
} // namespace

QSizeF valueLabelSize(const QString& text, qreal pixelSize)
{
    const QFontMetricsF fm(labelFont(pixelSize));
    return QSizeF(fm.horizontalAdvance(text) + pixelSize * 0.8, fm.height() + pixelSize * 0.3);
}

QPointF labelBesideSegment(const QPointF& a, const QPointF& b, const QString& text, qreal gap, qreal pixelSize)
{
    const QPointF mid = (a + b) / 2.0;
    const QPointF d = b - a;
    const qreal len = std::hypot(d.x(), d.y());
    QPointF n = len > 1e-9 ? QPointF(-d.y() / len, d.x() / len) : QPointF(0, -1);
    if (n.y() > 0 || (std::abs(n.y()) < 1e-9 && n.x() > 0))
        n = -n; // above, or to the left of vertical segments
    const QSizeF size = valueLabelSize(text, pixelSize);
    const qreal extent = std::abs(n.x()) * size.width() / 2 + std::abs(n.y()) * size.height() / 2;
    return mid + n * (gap + extent);
}

void paintValueLabel(QPainter& p, const QPointF& anchor, const QString& text, const QColor& color,
                     qreal counterRotation, qreal pixelSize)
{
    if (text.isEmpty())
        return;
    p.save();
    p.translate(anchor);
    if (counterRotation != 0.0)
        p.rotate(-counterRotation);
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(pixelSize));
    f.setBold(true);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const qreal w = fm.horizontalAdvance(text) + pixelSize * 0.8;
    const qreal h = fm.height() + pixelSize * 0.3;
    const QRectF box(-w / 2, -h / 2, w, h);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(12, 16, 14, 190));
    p.drawRoundedRect(box, h * 0.3, h * 0.3);
    p.setPen(color);
    p.drawText(box, Qt::AlignCenter, text);
    p.restore();
}

} // namespace cb
