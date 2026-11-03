#pragma once

#include "document/DocumentObject.h"

namespace cb {

/// A picture placed on a page. Pixel data lives once in the document ImageStore (shared by every
/// copy, duplicate and undo snapshot); the object only stores the asset key and display size.
class ImageObject final : public DocumentObject
{
public:
    ImageObject();
    static std::unique_ptr<ImageObject> create(const QString& key, const QSizeF& displaySize, const QPointF& center);

    const QString& imageKey() const { return m_key; }
    QSizeF displaySize() const { return m_size; }

    QRectF localBounds() const override;
    void paint(QPainter& painter, const RenderContext& ctx) const override;
    bool keepAspectRatio() const override { return true; }
    void collectImageKeys(QSet<QString>& keys) const override { keys.insert(m_key); }
    std::unique_ptr<DocumentObject> clone() const override;

protected:
    void applyResize(const QSizeF& newSize, ResizeHint hint) override;
    void writeProperties(QJsonObject& obj) const override;
    bool readProperties(const QJsonObject& obj) override;

private:
    ImageObject(const ImageObject&) = default;
    QString m_key;
    QSizeF m_size{200, 150};
};

} // namespace cb
