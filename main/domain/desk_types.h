#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DESK_AXIS_COUNT 3U

typedef enum {
    DESK_AXIS_FRONT_LEFT = 0,
    DESK_AXIS_FRONT_RIGHT,
    DESK_AXIS_WING,
} desk_axis_t;

typedef enum {
    MOTOR_COMMAND_STOP = 0,
    MOTOR_COMMAND_UP,
    MOTOR_COMMAND_DOWN,
} motor_command_t;

typedef enum {
    DESK_STATE_SAFE_START = 0,
    DESK_STATE_UNREFERENCED,
    DESK_STATE_IDLE,
    DESK_STATE_MOVING_UP,
    DESK_STATE_MOVING_DOWN,
    DESK_STATE_CALIBRATING_DOWN,
    DESK_STATE_ERROR,
} desk_state_t;

typedef enum {
    DESK_ERROR_NONE = 0,
    DESK_ERROR_EMERGENCY_STOP,
    DESK_ERROR_BOTH_BUTTONS_PRESSED,
    DESK_ERROR_ALIGNMENT_TIMEOUT,
    DESK_ERROR_CALIBRATION_TIMEOUT,
    DESK_ERROR_LIMIT_CONFLICT,
    DESK_ERROR_INVALID_TRANSITION,
    DESK_ERROR_HARDWARE_FAULT,
    DESK_ERROR_CONTROL_LOOP_OVERRUN,
    DESK_ERROR_FEEDBACK_MISSING,
    DESK_ERROR_FEEDBACK_DIRECTION,
    DESK_ERROR_FEEDBACK_STALL,
    DESK_ERROR_FEEDBACK_SKEW,
} desk_error_t;

typedef struct {
    bool upper_limit_active;
    bool lower_limit_active;
    int32_t hall_count;
    int32_t position_um;
    bool hall_valid;
} desk_axis_inputs_t;

typedef struct {
    desk_axis_inputs_t axis[DESK_AXIS_COUNT];
    bool up_pressed;
    bool down_pressed;
    bool calibrate_pressed;
    bool emergency_active;
    bool hardware_fault;
} desk_inputs_t;

typedef struct {
    motor_command_t direction;
    uint16_t duty_permille;
    bool enable;
} desk_axis_output_t;

typedef struct {
    desk_axis_output_t motor[DESK_AXIS_COUNT];
    desk_state_t state;
    desk_error_t error;
    bool feedback_warning;
    bool calibration_completed;
    bool position_reference_valid;
} desk_outputs_t;
