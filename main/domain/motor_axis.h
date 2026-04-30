#pragma once

#include <stdbool.h>

#include "desk_types.h"

typedef struct {
    bool rpwm_active;
    bool lpwm_active;
    bool enable_active;
} motor_axis_output_t;

motor_axis_output_t motor_axis_command_to_output(motor_command_t command);
bool motor_axis_command_is_valid(motor_command_t command);
bool motor_axis_output_is_safe(const motor_axis_output_t *output);
bool motor_axis_output_is_coherent(const motor_axis_output_t *output);
