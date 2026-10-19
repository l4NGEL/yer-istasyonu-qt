#pragma once
// MAVLink'in kullandığı CRC-16/MCRF4XX (X.25) sağlama toplamı.

#include <cstddef>
#include <cstdint>

namespace gcs::mav {

inline void crcAccumulate(std::uint8_t byte, std::uint16_t& crc) {
    std::uint8_t tmp = static_cast<std::uint8_t>(byte ^ static_cast<std::uint8_t>(crc & 0xFF));
    tmp = static_cast<std::uint8_t>(tmp ^ (tmp << 4));
    crc = static_cast<std::uint16_t>((crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4));
}

inline std::uint16_t crcCalculate(const std::uint8_t* data, std::size_t len,
                                  std::uint16_t crc = 0xFFFF) {
    for (std::size_t i = 0; i < len; ++i) crcAccumulate(data[i], crc);
    return crc;
}

}  // namespace gcs::mav
