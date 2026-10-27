#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace tactlink {

/**
 * @brief Fast IEEE 802.3 CRC-32 Engine using precomputed 256-entry lookup table.
 * Essential for physical RF layer bit-error detection under jamming/fading.
 */
class CRC32 {
public:
    static uint32_t calculate(const uint8_t* data, size_t length) {
        ensure_table_init();
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i) {
            uint8_t table_idx = static_cast<uint8_t>((crc ^ data[i]) & 0xFF);
            crc = (crc >> 8) ^ table_[table_idx];
        }
        return crc ^ 0xFFFFFFFF;
    }

private:
    static void ensure_table_init() {
        static bool initialized = false;
        if (!initialized) {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int j = 0; j < 8; ++j) {
                    if (c & 1) {
                        c = 0xEDB88320 ^ (c >> 1);
                    } else {
                        c = c >> 1;
                    }
                }
                table_[i] = c;
            }
            initialized = true;
        }
    }

    static uint32_t table_[256];
};

__attribute__((selectany)) uint32_t CRC32::table_[256] = {0};

} // namespace tactlink
