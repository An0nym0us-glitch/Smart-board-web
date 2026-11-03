#include "document/ImageObject.h"

#include "core/JsonUtil.h"
#include "document/ImageStore.h"

#include <QPainter>

#include <algorithm>

namespace cb {

ImageObject::ImageObject()
    : DocumentObject(ObjectType::Image)
{
}

std::unique_ptr<ImageObject> ImageObject::create(const QString& key, const QSizeF& displaySize, const QPointF& center)
{
    auto img = std::make_unique<ImageObject>();
    img->m_key = key;
    img->m_size = QSizeF(std::max(4.0, displaySize.width()), std::max(4.0, displaySize.height()));
    img->setPosition(center);
    return img;
}

QRectF ImageObject::localBounds() const
{
    return QRectF(-m_size.width() / 2, -m_size.height() / 2, m_size.width(), m_size.height());
}

void ImageObject::paint(QPainter& painter, const RenderContext& ctx) const
{
    const QRectF target = localBounds();
    if (ctx.images && ctx.images->contains(m_key)) {
        // Request a level of detail matching the on-screen size (full resolution for export).
        const qreal scale = ctx.exporting ? 4.0 : std::max(0.05, ctx.zoom * 2.0);
        const QImage img = ctx.images->imageForSize(m_key, target.size() * scale);
        painter.save();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(target, img);
        painter.restore();
        return;
    }
    painter.save();
    painter.setPen(QPen(QColor(255, 255, 255, 120), 2, Qt::DashLine));
    painter.setBrush(QColor(255, 255, 255, 20));
    painter.drawRect(target);
    painter.drawLine(target.topLeft(), target.bottomRight());
    painter.drawLine(target.topRight(), target.bottomLeft());
    painter.restore();
}

std::unique_ptr<DocumentObject> ImageObject::clone() const
{
    return std::unique_ptr<DocumentObject>(new ImageObject(*this));
}

void ImageObject::applyResize(const QSizeF& newSize, ResizeHint hint)
{
    Q_UNUSED(hint);
    m_size = newSize;
}

void ImageObject::writeProperties(QJsonObject& obj) const
{
    obj.insert(QStringLiteral("image"), m_key);
    obj.insert(QStringLiteral("size"), json::fromSize(m_size));
}

bool ImageObject::readProperties(const QJsonObject& obj)
{
    m_key = obj.value(QStringLiteral("image")).toString();
    m_size = json::toSize(obj.value(QStringLiteral("size")), QSizeF(200, 150));
    return !m_key.isEmpty();
}

} // namespace cb
