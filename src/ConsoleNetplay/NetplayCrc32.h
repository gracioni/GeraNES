#pragma once

#include <cstddef>
#include <cstdint>

namespace ConsoleNetplay {

inline uint32_t crc32(const void* data, size_t size)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = ~0u;
    for(size_t i = 0; i < size; ++i) {
        uint32_t ch = bytes[i];
        for(size_t bit = 0; bit < 8; ++bit) {
            const uint32_t carry = (ch ^ crc) & 1u;
            crc >>= 1u;
            if(carry != 0u) crc ^= 0xEDB88320u;
            ch >>= 1u;
        }
    }
    return ~crc;
}

} // namespace ConsoleNetplay
