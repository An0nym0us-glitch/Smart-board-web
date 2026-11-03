#include "document/ImageStore.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QMutexLocker>

#include <algorithm>
#include <cmath>

namespace cb {

QString ImageStore::addEncoded(const QByteArray& bytes, QString* error)
{
    if (bytes.isEmpty()) {
        if (error)
            *error = QStringLiteral("Empty image data");
        return QString();
    }
    const QString key = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
    if (m_entries.contains(key))
        return key;

    QBuffer buffer;
    buffer.setData(bytes);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    reader.setAutoTransform(true);
    const QByteArray format = reader.format();
    QSize size = reader.size();
    const QSize original = size;
    if (size.isValid() && std::max(size.width(), size.height()) > kMaxDecodedSide) {
        size.scale(kMaxDecodedSide, kMaxDecodedSide, Qt::KeepAspectRatio);
        reader.setScaledSize(size);
    }
    QImage image = reader.read();
    if (image.isNull()) {
        if (error)
            *error = reader.errorString();
        return QString();
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied && image.format() != QImage::Format_RGB32)
        image = image.convertToFormat(image.hasAlphaChannel() ? QImage::Format_ARGB32_Premultiplied
                                                              : QImage::Format_RGB32);

    auto entry = std::make_shared<Entry>();
    entry->encoded = bytes;
    entry->suffix = format.isEmpty() ? QStringLiteral("png") : QString::fromLatin1(format).toLower();
    entry->image = image;
    entry->originalSize = original.isValid() ? original : image.size();
    m_entries.insert(key, entry);
    return key;
}

QString ImageStore::addImage(const QImage& image)
{
    if (image.isNull())
        return QString();
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return addEncoded(bytes);
}

QImage ImageStore::image(const QString& key) const
{
    const auto it = m_entries.constFind(key);
    return it == m_entries.constEnd() ? QImage() : (*it)->image;
}

QImage ImageStore::imageForSize(const QString& key, const QSizeF& deviceSize) const
{
    const auto it = m_entries.constFind(key);
    if (it == m_entries.constEnd())
        return QImage();
    const Entry& e = **it;
    const int fullSide = std::max(e.image.width(), e.image.height());
    const double wanted = std::max(deviceSize.width(), deviceSize.height());
    // Choose the smallest power-of-two level that is at least as large as needed.
    int side = fullSide;
    while (side / 2 >= wanted && side / 2 >= 64)
        side /= 2;
    if (side >= fullSide)
        return e.image;
    QMutexLocker lock(&e.lodMutex);
    auto lod = e.lods.constFind(side);
    if (lod != e.lods.constEnd())
        return *lod;
    const QImage scaled = e.image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    e.lods.insert(side, scaled);
    return scaled;
}

QByteArray ImageStore::encodedData(const QString& key) const
{
    const auto it = m_entries.constFind(key);
    return it == m_entries.constEnd() ? QByteArray() : (*it)->encoded;
}

QString ImageStore::suffix(const QString& key) const
{
    const auto it = m_entries.constFind(key);
    return it == m_entries.constEnd() ? QString() : (*it)->suffix;
}

QSize ImageStore::originalSize(const QString& key) const
{
    const auto it = m_entries.constFind(key);
    return it == m_entries.constEnd() ? QSize() : (*it)->originalSize;
}

void ImageStore::retainOnly(const QSet<QString>& used)
{
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (!used.contains(it.key()))
            it = m_entries.erase(it);
        else
            ++it;
    }
}

void ImageStore::mergeFrom(const ImageStore& other)
{
    for (auto it = other.m_entries.constBegin(); it != other.m_entries.constEnd(); ++it)
        if (!m_entries.contains(it.key()))
            m_entries.insert(it.key(), it.value());
}

} // namespace cb
