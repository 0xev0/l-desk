#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_types.h"

typedef struct {
    bool rpwm_active;
    bool lpwm_active;
    bool enable_active;
    uint16_t duty_permille;
} motor_axis_output_t;

motor_axis_output_t motor_axis_command_to_output(const desk_axis_output_t *command);
bool motor_axis_command_is_valid(const desk_axis_output_t *command);
bool motor_axis_output_is_safe(const motor_axis_output_t *output);
bool motor_axis_output_is_coherent(const motor_axis_output_t *output);
