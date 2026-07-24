#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_preset_controller.h"

#define DESK_PRESET_RECORD_SIZE 20U

bool desk_preset_record_encode(const desk_preset_bank_t *bank, int32_t maximum_travel_um,
                               uint8_t record[DESK_PRESET_RECORD_SIZE]);
bool desk_preset_record_decode(const uint8_t *record, size_t record_size,
                               int32_t maximum_travel_um, desk_preset_bank_t *bank);
