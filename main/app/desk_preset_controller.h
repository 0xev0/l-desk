#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_types.h"

#define DESK_PRESET_COUNT 3U
#define DESK_PRESET_POSITION_TOLERANCE_UM 1000

typedef struct {
    int32_t target_um[DESK_PRESET_COUNT];
    bool valid[DESK_PRESET_COUNT];
} desk_preset_bank_t;

typedef struct {
    size_t slot;
    int32_t target_um;
    motor_command_t direction;
    bool target_reached;
    bool active;
} desk_preset_controller_t;

void desk_preset_controller_reset(desk_preset_controller_t *controller);
bool desk_preset_bank_set(desk_preset_bank_t *bank, size_t slot, int32_t target_um,
                          int32_t maximum_travel_um);
bool desk_preset_bank_get(const desk_preset_bank_t *bank, size_t slot, int32_t *target_um);
bool desk_preset_controller_update(
    desk_preset_controller_t *controller, const desk_preset_bank_t *bank, size_t slot,
    const desk_inputs_t *inputs, bool position_reference_valid, motor_command_t *direction);
