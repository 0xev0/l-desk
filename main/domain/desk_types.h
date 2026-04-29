#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DESK_AXIS_LEFT = 0,
    DESK_AXIS_RIGHT = 1,
} desk_axis_t;

typedef enum {
    MOTOR_COMMAND_STOP = 0,
    MOTOR_COMMAND_UP,
    MOTOR_COMMAND_DOWN,
} motor_command_t;

typedef enum {
    DESK_STATE_IDLE = 0,
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
    DESK_ERROR_LIMIT_CONFLICT,
    DESK_ERROR_INVALID_TRANSITION,
    DESK_ERROR_HARDWARE_FAULT,
} desk_error_t;

typedef struct {
    bool up_pressed;
    bool down_pressed;
    bool calibrate_pressed;
    bool emergency_active;
    bool left_upper_limit_active;
    bool left_lower_limit_active;
    bool right_upper_limit_active;
    bool right_lower_limit_active;
} desk_inputs_t;

typedef struct {
    motor_command_t left_motor;
    motor_command_t right_motor;
    desk_state_t state;
    desk_error_t error;
} desk_outputs_t;
