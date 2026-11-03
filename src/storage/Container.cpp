#include "storage/Container.h"

#include "core/Crc32.h"

#include <QCoreApplication>
#include <QDataStream>

namespace cb {

namespace {
const char kMagic[8] = {'C', 'L', 'A', 'S', 'S', 'B', 'R', 'D'};
QString tr(const char* s) { return QCoreApplication::translate("Container", s); }
} // namespace

void Container::add(const QString& name, const QByteArray& data, bool compress)
{
    m_entries.push_back({name, data, compress});
}

const Container::Entry* Container::find(const QString& name) const
{
    for (const Entry& e : m_entries)
        if (e.name == name)
            return &e;
    return nullptr;
}

bool Container::looksLikeContainer(const QByteArray& head)
{
    return head.size() >= 8 && std::equal(kMagic, kMagic + 8, head.constData());
}

QByteArray Container::write() const
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s.setVersion(QDataStream::Qt_5_15);
    s.writeRawData(kMagic, 8);
    s << kVersion << quint32(m_entries.size());
    for (const Entry& e : m_entries) {
        const quint8 flags = e.compress ? 1 : 0;
        s << e.name << flags << quint32(crc32(e.data));
        s << (e.compress ? qCompress(e.data, 6) : e.data);
    }
    return out;
}

bool Container::read(const QByteArray& bytes, QString* error)
{
    m_entries.clear();
    auto fail = [&](const QString& msg) {
        if (error)
            *error = msg;
        m_entries.clear();
        return false;
    };
    if (!looksLikeContainer(bytes))
        return fail(tr("This is not a ClassBoard lesson file."));
    QDataStream s(bytes);
    s.setVersion(QDataStream::Qt_5_15);
    s.skipRawData(8);
    quint32 version = 0;
    quint32 count = 0;
    s >> version >> count;
    if (s.status() != QDataStream::Ok)
        return fail(tr("The lesson file is truncated."));
    if (version > kVersion)
        return fail(tr("The lesson file was created by a newer version of ClassBoard."));
    if (count > 1000000)
        return fail(tr("The lesson file is corrupted."));
    for (quint32 i = 0; i < count; ++i) {
        Entry e;
        quint8 flags = 0;
        quint32 crc = 0;
        QByteArray data;
        s >> e.name >> flags >> crc >> data;
        if (s.status() != QDataStream::Ok)
            return fail(tr("The lesson file is truncated."));
        e.compress = flags & 1;
        e.data = e.compress ? qUncompress(data) : data;
        if (e.compress && e.data.isEmpty() && !data.isEmpty())
            return fail(tr("Part '%1' of the lesson file is corrupted.").arg(e.name));
        if (crc32(e.data) != crc)
            return fail(tr("Part '%1' of the lesson file is corrupted.").arg(e.name));
        m_entries.push_back(e);
    }
    return true;
}

} // namespace cb
