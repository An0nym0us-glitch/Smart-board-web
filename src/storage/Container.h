#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace cb {

/// Minimal, versioned archive used by .classboard files.
///
/// Layout (big endian, QDataStream Qt 5.15):
///   "CLASSBRD" | quint32 containerVersion | quint32 entryCount |
///   entryCount x { QString name | quint8 flags | quint32 crc32 | QByteArray data }
/// flags bit 0: data is zlib compressed (qCompress). The CRC covers the uncompressed bytes so
/// truncated or corrupted files are detected instead of silently loading garbage.
class Container
{
public:
    static constexpr quint32 kVersion = 1;

    struct Entry
    {
        QString name;
        QByteArray data;
        bool compress = false;
    };

    void add(const QString& name, const QByteArray& data, bool compress);
    const QVector<Entry>& entries() const { return m_entries; }
    const Entry* find(const QString& name) const;

    QByteArray write() const;
    bool read(const QByteArray& bytes, QString* error);

    static bool looksLikeContainer(const QByteArray& head);

private:
    QVector<Entry> m_entries;
};

} // namespace cb
