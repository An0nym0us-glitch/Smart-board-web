#pragma once

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <QString>

#include <memory>

namespace cb {

/// Content-addressed store of image assets shared by every page of a document.
///
/// Images are deduplicated by the SHA-1 of their encoded bytes. The encoded bytes are kept so that
/// saving never re-encodes (and never loses quality); the decoded image is limited in size to bound
/// memory, and down-scaled levels of detail are generated lazily for fast canvas rendering.
/// Copies of an ImageStore share the underlying entries, which are immutable after creation and
/// therefore safe to use from export threads.
class ImageStore
{
public:
    static constexpr int kMaxDecodedSide = 4096;

    /// Adds encoded image data (PNG, JPEG, ...). Returns the key or an empty string on failure.
    QString addEncoded(const QByteArray& bytes, QString* error = nullptr);

    /// Adds a decoded image by encoding it as PNG.
    QString addImage(const QImage& image);

    bool contains(const QString& key) const { return m_entries.contains(key); }
    QImage image(const QString& key) const;
    /// Returns an image suitable for drawing at the given device pixel size (level of detail).
    QImage imageForSize(const QString& key, const QSizeF& deviceSize) const;
    QByteArray encodedData(const QString& key) const;
    QString suffix(const QString& key) const;
    QSize originalSize(const QString& key) const;

    QStringList keys() const { return m_entries.keys(); }
    int count() const { return m_entries.size(); }

    /// Drops assets not referenced by the given keys.
    void retainOnly(const QSet<QString>& used);

    /// Merges assets of another store (shared, no copies).
    void mergeFrom(const ImageStore& other);

    void clear() { m_entries.clear(); }

private:
    struct Entry
    {
        QByteArray encoded;
        QString suffix;
        QImage image;
        QSize originalSize;
        mutable QMutex lodMutex;
        mutable QHash<int, QImage> lods; // keyed by max side in pixels
    };
    QHash<QString, std::shared_ptr<const Entry>> m_entries;
};

} // namespace cb
