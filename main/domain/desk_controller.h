#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_config.h"
#include "desk_types.h"

typedef struct {
    desk_config_t config;
    desk_state_t state;
    desk_error_t error;
    desk_axis_output_t motor[DESK_AXIS_COUNT];
    motor_command_t last_motion_direction;
    motor_command_t pending_direction;
    motor_command_t debounce_candidate_direction;
    uint64_t last_tick_ms;
    uint64_t safe_start_since_ms;
    uint64_t direction_stopped_at_ms;
    uint64_t deadtime_until_ms;
    uint64_t button_candidate_since_ms;
    uint64_t calibrate_pressed_since_ms;
    uint64_t calibration_started_at_ms;
    uint64_t alignment_started_at_ms;
    uint64_t motion_started_at_ms;
    uint64_t feedback_changed_at_ms[DESK_AXIS_COUNT];
    int32_t previous_hall_count[DESK_AXIS_COUNT];
    bool axis_at_endstop[DESK_AXIS_COUNT];
    bool feedback_initialized;
    bool tick_initialized;
    bool safe_start_timing;
    bool calibrate_timing;
    bool alignment_active;
    bool motion_was_active;
    bool feedback_warning;
} desk_controller_t;

void desk_controller_init(desk_controller_t *controller, const desk_config_t *config);
desk_outputs_t desk_controller_tick(desk_controller_t *controller, const desk_inputs_t *inputs,
                                    uint64_t now_ms);
bool desk_controller_request_clear_error(desk_controller_t *controller, const desk_inputs_t *inputs,
                                         uint64_t now_ms);
void desk_controller_report_hardware_fault(desk_controller_t *controller, uint64_t now_ms);
