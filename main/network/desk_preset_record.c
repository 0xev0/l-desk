#include "desk_preset_record.h"

#include <string.h>

#define DESK_PRESET_RECORD_VERSION 1U
#define DESK_PRESET_RECORD_HEADER_SIZE 4U
#define DESK_PRESET_RECORD_CRC_OFFSET 16U
#define DESK_PRESET_RECORD_VALID_MASK 0x07U
#define DESK_CRC32_POLYNOMIAL UINT32_C(0xedb88320)

static uint32_t crc32(const uint8_t *data, size_t length)
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

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
    destination[2] = (uint8_t)(value >> 16U);
    destination[3] = (uint8_t)(value >> 24U);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) |
           ((uint32_t)source[2] << 16U) | ((uint32_t)source[3] << 24U);
}

bool desk_preset_record_encode(const desk_preset_bank_t *bank, int32_t maximum_travel_um,
                               uint8_t record[DESK_PRESET_RECORD_SIZE])
{
    if (bank == NULL || record == NULL || maximum_travel_um <= 0) {
        return false;
    }

    uint8_t encoded[DESK_PRESET_RECORD_SIZE] = {
        [0] = DESK_PRESET_RECORD_VERSION,
    };
    for (size_t slot = 0U; slot < DESK_PRESET_COUNT; ++slot) {
        int32_t target_um = 0;
        if (bank->valid[slot]) {
            if (!desk_preset_bank_get(bank, slot, &target_um) || target_um < 0 ||
                target_um > maximum_travel_um) {
                return false;
            }
            encoded[1] |= (uint8_t)(1U << slot);
        }
        write_u32_le(&encoded[DESK_PRESET_RECORD_HEADER_SIZE + (slot * sizeof(uint32_t))],
                     (uint32_t)target_um);
    }
    write_u32_le(&encoded[DESK_PRESET_RECORD_CRC_OFFSET],
                 crc32(encoded, DESK_PRESET_RECORD_CRC_OFFSET));
    memcpy(record, encoded, sizeof(encoded));
    return true;
}

bool desk_preset_record_decode(const uint8_t *record, size_t record_size,
                               int32_t maximum_travel_um, desk_preset_bank_t *bank)
{
    if (record == NULL || bank == NULL || record_size != DESK_PRESET_RECORD_SIZE ||
        maximum_travel_um <= 0 || record[0] != DESK_PRESET_RECORD_VERSION ||
        (record[1] & (uint8_t)~DESK_PRESET_RECORD_VALID_MASK) != 0U ||
        record[2] != 0U || record[3] != 0U ||
        read_u32_le(&record[DESK_PRESET_RECORD_CRC_OFFSET]) !=
            crc32(record, DESK_PRESET_RECORD_CRC_OFFSET)) {
        return false;
    }

    desk_preset_bank_t decoded = {0};
    for (size_t slot = 0U; slot < DESK_PRESET_COUNT; ++slot) {
        uint32_t target =
            read_u32_le(&record[DESK_PRESET_RECORD_HEADER_SIZE + (slot * sizeof(uint32_t))]);
        bool valid = (record[1] & (uint8_t)(1U << slot)) != 0U;
        if ((!valid && target != 0U) || target > (uint32_t)maximum_travel_um ||
            (valid && !desk_preset_bank_set(&decoded, slot, (int32_t)target,
                                            maximum_travel_um))) {
            return false;
        }
    }
    *bank = decoded;
    return true;
}
