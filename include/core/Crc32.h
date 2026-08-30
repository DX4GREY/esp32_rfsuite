#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Crc32 {

inline uint32_t begin() { return 0xFFFFFFFFUL; }

inline uint32_t update(uint32_t crc, const uint8_t* data, size_t length) {
    while (length--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
    return crc;
}

inline uint32_t finish(uint32_t crc) { return crc ^ 0xFFFFFFFFUL; }

inline uint32_t calculate(const void* data, size_t length) {
    return finish(update(begin(), static_cast<const uint8_t*>(data), length));
}

}  // namespace Crc32
