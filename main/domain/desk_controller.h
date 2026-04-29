#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_config.h"
#include "desk_types.h"

typedef struct {
    desk_config_t config;
    desk_state_t state;
    desk_error_t error;
    motor_command_t left_motor;
    motor_command_t right_motor;
    motor_command_t last_motion_direction;
    motor_command_t pending_direction;
    uint32_t deadtime_remaining_ms;
    uint32_t alignment_elapsed_ms;
    uint32_t calibration_elapsed_ms;
    bool alignment_active;
    bool left_aligned;
    bool right_aligned;
} desk_controller_t;

void desk_controller_init(desk_controller_t *controller, const desk_config_t *config);
desk_outputs_t desk_controller_tick(desk_controller_t *controller, const desk_inputs_t *inputs, uint32_t elapsed_ms);
void desk_controller_request_clear_error(desk_controller_t *controller);
