#include "desk_crc32.h"

#define DESK_CRC32_POLYNOMIAL UINT32_C(0xedb88320)

uint32_t desk_crc32_compute(const uint8_t *data, size_t length)
{
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (unsigned bit = 0U; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1U));
            crc = (crc >> 1U) ^ (DESK_CRC32_POLYNOMIAL & mask);
        }
    }
    return ~crc;
}
