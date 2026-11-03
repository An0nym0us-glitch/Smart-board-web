#include "export/ZipWriter.h"

#include "core/Crc32.h"

#include <QDateTime>

namespace cb {

namespace {
void put16(QByteArray& b, quint16 v)
{
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void put32(QByteArray& b, quint32 v)
{
    for (int i = 0; i < 4; ++i)
        b.append(char((v >> (8 * i)) & 0xFF));
}

void dosDateTime(quint16& date, quint16& time)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate d = now.date();
    const QTime t = now.time();
    date = quint16(((d.year() - 1980) << 9) | (d.month() << 5) | d.day());
    time = quint16((t.hour() << 11) | (t.minute() << 5) | (t.second() / 2));
}

/// Raw deflate stream from qCompress output (strip Qt's size prefix, zlib header and checksum).
QByteArray rawDeflate(const QByteArray& data)
{
    const QByteArray z = qCompress(data, 9);
    if (z.size() < 4 + 2 + 4)
        return QByteArray();
    return z.mid(4 + 2, z.size() - 4 - 2 - 4);
}
} // namespace

void ZipWriter::addFile(const QString& name, const QByteArray& data, bool compress)
{
    Entry e;
    e.name = name.toUtf8();
    e.crc = crc32(data);
    e.size = static_cast<quint32>(data.size());
    QByteArray payload = data;
    e.method = 0;
    if (compress && !data.isEmpty()) {
        const QByteArray deflated = rawDeflate(data);
        if (!deflated.isEmpty() && deflated.size() < data.size()) {
            payload = deflated;
            e.method = 8;
        }
    }
    e.compressedSize = static_cast<quint32>(payload.size());
    e.offset = static_cast<quint32>(m_out.size());

    quint16 date = 0, time = 0;
    dosDateTime(date, time);
    put32(m_out, 0x04034b50);
    put16(m_out, 20);
    put16(m_out, 0x0800); // UTF-8 names
    put16(m_out, e.method);
    put16(m_out, time);
    put16(m_out, date);
    put32(m_out, e.crc);
    put32(m_out, e.compressedSize);
    put32(m_out, e.size);
    put16(m_out, static_cast<quint16>(e.name.size()));
    put16(m_out, 0);
    m_out.append(e.name);
    m_out.append(payload);
    m_entries.push_back(e);
}

QByteArray ZipWriter::finish()
{
    quint16 date = 0, time = 0;
    dosDateTime(date, time);
    const quint32 cdOffset = static_cast<quint32>(m_out.size());
    for (const Entry& e : m_entries) {
        put32(m_out, 0x02014b50);
        put16(m_out, 20);
        put16(m_out, 20);
        put16(m_out, 0x0800);
        put16(m_out, e.method);
        put16(m_out, time);
        put16(m_out, date);
        put32(m_out, e.crc);
        put32(m_out, e.compressedSize);
        put32(m_out, e.size);
        put16(m_out, static_cast<quint16>(e.name.size()));
        put16(m_out, 0); // extra
        put16(m_out, 0); // comment
        put16(m_out, 0); // disk
        put16(m_out, 0); // internal attributes
        put32(m_out, 0); // external attributes
        put32(m_out, e.offset);
        m_out.append(e.name);
    }
    const quint32 cdSize = static_cast<quint32>(m_out.size()) - cdOffset;
    put32(m_out, 0x06054b50);
    put16(m_out, 0);
    put16(m_out, 0);
    put16(m_out, static_cast<quint16>(m_entries.size()));
    put16(m_out, static_cast<quint16>(m_entries.size()));
    put32(m_out, cdSize);
    put32(m_out, cdOffset);
    put16(m_out, 0);
    QByteArray result;
    result.swap(m_out);
    m_entries.clear();
    return result;
}

} // namespace cb
