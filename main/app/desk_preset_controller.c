#include "desk_preset_controller.h"

#include <stddef.h>

void desk_preset_controller_reset(desk_preset_controller_t *controller)
{
    if (controller != NULL) {
        *controller = (desk_preset_controller_t){0};
    }
}

bool desk_preset_bank_set(desk_preset_bank_t *bank, size_t slot, int32_t target_um,
                          int32_t maximum_travel_um)
{
    if (bank == NULL || slot >= DESK_PRESET_COUNT || maximum_travel_um <= 0 || target_um < 0 ||
        target_um > maximum_travel_um) {
        return false;
    }
    bank->target_um[slot] = target_um;
    bank->valid[slot] = true;
    return true;
}

bool desk_preset_bank_get(const desk_preset_bank_t *bank, size_t slot, int32_t *target_um)
{
    if (bank == NULL || slot >= DESK_PRESET_COUNT || target_um == NULL || !bank->valid[slot]) {
        return false;
    }
    *target_um = bank->target_um[slot];
    return true;
}

static bool average_position(const desk_inputs_t *inputs, int32_t *average_um)
{
    int64_t sum = 0;
    for (size_t axis = 0U; axis < DESK_AXIS_COUNT; ++axis) {
        if (!inputs->axis[axis].hall_valid) {
            return false;
        }
        sum += inputs->axis[axis].position_um;
    }
    *average_um = (int32_t)(sum / (int64_t)DESK_AXIS_COUNT);
    return true;
}

bool desk_preset_bank_capture(desk_preset_bank_t *bank, size_t slot, const desk_inputs_t *inputs,
                              bool position_reference_valid, bool feedback_warning,
                              int32_t maximum_travel_um, int32_t maximum_skew_um)
{
    if (bank == NULL || inputs == NULL || slot >= DESK_PRESET_COUNT || !position_reference_valid ||
        feedback_warning || maximum_travel_um <= 0 || maximum_skew_um < 0) {
        return false;
    }

    int32_t minimum = maximum_travel_um;
    int32_t maximum = 0;
    for (size_t axis = 0U; axis < DESK_AXIS_COUNT; ++axis) {
        int32_t position = inputs->axis[axis].position_um;
        if (!inputs->axis[axis].hall_valid || position < 0 || position > maximum_travel_um) {
            return false;
        }
        if (position < minimum) {
            minimum = position;
        }
        if (position > maximum) {
            maximum = position;
        }
    }
    if ((int64_t)maximum - minimum > maximum_skew_um) {
        return false;
    }

    int32_t average_um;
    return average_position(inputs, &average_um) &&
           desk_preset_bank_set(bank, slot, average_um, maximum_travel_um);
}

static bool target_has_been_reached(const desk_preset_controller_t *controller, int32_t current_um)
{
    if (controller->direction == MOTOR_COMMAND_UP) {
        return current_um >= controller->target_um - DESK_PRESET_POSITION_TOLERANCE_UM;
    }
    if (controller->direction == MOTOR_COMMAND_DOWN) {
        return current_um <= controller->target_um + DESK_PRESET_POSITION_TOLERANCE_UM;
    }
    return true;
}

bool desk_preset_controller_update(desk_preset_controller_t *controller,
                                   const desk_preset_bank_t *bank, size_t slot,
                                   const desk_inputs_t *inputs, bool position_reference_valid,
                                   motor_command_t *direction)
{
    if (controller == NULL || bank == NULL || inputs == NULL || direction == NULL ||
        !position_reference_valid || slot >= DESK_PRESET_COUNT) {
        return false;
    }

    int32_t target_um;
    int32_t current_um;
    if (!desk_preset_bank_get(bank, slot, &target_um) || !average_position(inputs, &current_um)) {
        return false;
    }

    if (!controller->active || controller->slot != slot) {
        *controller = (desk_preset_controller_t){
            .slot = slot,
            .target_um = target_um,
            .direction = current_um < target_um ? MOTOR_COMMAND_UP : MOTOR_COMMAND_DOWN,
            .active = true,
        };
        int64_t difference = (int64_t)target_um - current_um;
        if (difference >= -DESK_PRESET_POSITION_TOLERANCE_UM &&
            difference <= DESK_PRESET_POSITION_TOLERANCE_UM) {
            controller->direction = MOTOR_COMMAND_STOP;
            controller->target_reached = true;
        }
    }

    if (!controller->target_reached && target_has_been_reached(controller, current_um)) {
        controller->target_reached = true;
    }
    *direction = controller->target_reached ? MOTOR_COMMAND_STOP : controller->direction;
    return true;
}
