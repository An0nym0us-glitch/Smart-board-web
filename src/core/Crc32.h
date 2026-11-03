#pragma once

#include <QByteArray>

#include <cstdint>

namespace cb {

/// Standard CRC-32 (IEEE 802.3, as used by ZIP and PNG).
std::uint32_t crc32(const QByteArray& data, std::uint32_t crc = 0);

} // namespace cb
