#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace cb {

/// Minimal ZIP (PKWARE APPNOTE 2.0) writer for Office Open XML packages. Entries are stored or
/// deflated with zlib (via Qt), names are UTF-8.
class ZipWriter
{
public:
    void addFile(const QString& name, const QByteArray& data, bool compress = true);
    QByteArray finish();

private:
    struct Entry
    {
        QByteArray name;
        quint32 crc = 0;
        quint32 compressedSize = 0;
        quint32 size = 0;
        quint16 method = 0;
        quint32 offset = 0;
    };
    QByteArray m_out;
    QVector<Entry> m_entries;
};

} // namespace cb
