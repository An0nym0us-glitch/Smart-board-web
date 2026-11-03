#include "core/Crc32.h"

#include <array>

namespace cb {

namespace {
std::array<std::uint32_t, 256> makeTable()
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        table[i] = c;
    }
    return table;
}
} // namespace

std::uint32_t crc32(const QByteArray& data, std::uint32_t crc)
{
    static const std::array<std::uint32_t, 256> table = makeTable();
    crc = ~crc;
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.constData());
    for (int i = 0; i < data.size(); ++i)
        crc = table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

} // namespace cb
