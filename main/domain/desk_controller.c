#include "desk_controller.h"

#include <limits.h>
#include <stddef.h>

desk_config_t desk_config_default(void)
{
    return (desk_config_t){
        .control_tick_ms = 10,
        .direction_change_deadtime_ms = 300,
        .alignment_timeout_ms = 3000,
        .calibration_timeout_ms = 45000,
    };
}

static desk_outputs_t stopped_outputs(desk_state_t state, desk_error_t error)
{
    return (desk_outputs_t){
        .left_motor = MOTOR_COMMAND_STOP,
        .right_motor = MOTOR_COMMAND_STOP,
        .state = state,
        .error = error,
    };
}

static desk_outputs_t current_outputs(const desk_controller_t *controller)
{
    return (desk_outputs_t){
        .left_motor = controller->left_motor,
        .right_motor = controller->right_motor,
        .state = controller->state,
        .error = controller->error,
    };
}

static void command_stop(desk_controller_t *controller)
{
    controller->left_motor = MOTOR_COMMAND_STOP;
    controller->right_motor = MOTOR_COMMAND_STOP;
}

static void enter_error(desk_controller_t *controller, desk_error_t error)
{
    controller->state = DESK_STATE_ERROR;
    controller->error = error;
    command_stop(controller);
}

static bool has_limit_conflict(const desk_inputs_t *inputs)
{
    return (inputs->left_upper_limit_active && inputs->left_lower_limit_active) ||
           (inputs->right_upper_limit_active && inputs->right_lower_limit_active);
}

static void command_up_with_limits(desk_controller_t *controller, const desk_inputs_t *inputs)
{
    controller->left_motor = inputs->left_upper_limit_active ? MOTOR_COMMAND_STOP : MOTOR_COMMAND_UP;
    controller->right_motor = inputs->right_upper_limit_active ? MOTOR_COMMAND_STOP : MOTOR_COMMAND_UP;

    if (controller->left_motor == MOTOR_COMMAND_STOP && controller->right_motor == MOTOR_COMMAND_STOP) {
        controller->state = DESK_STATE_IDLE;
        controller->last_motion_direction = MOTOR_COMMAND_STOP;
    } else {
        controller->state = DESK_STATE_MOVING_UP;
        controller->last_motion_direction = MOTOR_COMMAND_UP;
    }
}

static void command_down_with_limits(desk_controller_t *controller, const desk_inputs_t *inputs)
{
    controller->left_motor = inputs->left_lower_limit_active ? MOTOR_COMMAND_STOP : MOTOR_COMMAND_DOWN;
    controller->right_motor = inputs->right_lower_limit_active ? MOTOR_COMMAND_STOP : MOTOR_COMMAND_DOWN;

    if (controller->left_motor == MOTOR_COMMAND_STOP && controller->right_motor == MOTOR_COMMAND_STOP) {
        controller->state = DESK_STATE_IDLE;
        controller->last_motion_direction = MOTOR_COMMAND_STOP;
    } else {
        controller->state = DESK_STATE_MOVING_DOWN;
        controller->last_motion_direction = MOTOR_COMMAND_DOWN;
    }
}

void desk_controller_init(desk_controller_t *controller, const desk_config_t *config)
{
    if (controller == NULL) {
        return;
    }

    controller->config = (config != NULL) ? *config : desk_config_default();
    controller->state = DESK_STATE_IDLE;
    controller->error = DESK_ERROR_NONE;
    controller->left_motor = MOTOR_COMMAND_STOP;
    controller->right_motor = MOTOR_COMMAND_STOP;
    controller->last_motion_direction = MOTOR_COMMAND_STOP;
    controller->pending_direction = MOTOR_COMMAND_STOP;
    controller->deadtime_remaining_ms = 0;
    controller->alignment_elapsed_ms = 0;
    controller->calibration_elapsed_ms = 0;
    controller->alignment_active = false;
    controller->left_aligned = false;
    controller->right_aligned = false;
}

desk_outputs_t desk_controller_tick(desk_controller_t *controller, const desk_inputs_t *inputs, uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    if (controller == NULL || inputs == NULL) {
        return stopped_outputs(DESK_STATE_ERROR, DESK_ERROR_HARDWARE_FAULT);
    }

    if (controller->state == DESK_STATE_ERROR) {
        command_stop(controller);
        return current_outputs(controller);
    }

    if (inputs->emergency_active) {
        enter_error(controller, DESK_ERROR_EMERGENCY_STOP);
        return current_outputs(controller);
    }

    if (has_limit_conflict(inputs)) {
        enter_error(controller, DESK_ERROR_LIMIT_CONFLICT);
        return current_outputs(controller);
    }

    if (inputs->up_pressed && inputs->down_pressed) {
        enter_error(controller, DESK_ERROR_BOTH_BUTTONS_PRESSED);
        return current_outputs(controller);
    }

    if (inputs->up_pressed) {
        controller->error = DESK_ERROR_NONE;
        command_up_with_limits(controller, inputs);
        return current_outputs(controller);
    }

    if (inputs->down_pressed) {
        controller->error = DESK_ERROR_NONE;
        command_down_with_limits(controller, inputs);
        return current_outputs(controller);
    }

    controller->state = DESK_STATE_IDLE;
    controller->error = DESK_ERROR_NONE;
    command_stop(controller);
    return current_outputs(controller);
}

void desk_controller_request_clear_error(desk_controller_t *controller)
{
    if (controller == NULL || controller->state != DESK_STATE_ERROR) {
        return;
    }

    controller->state = DESK_STATE_IDLE;
    controller->error = DESK_ERROR_NONE;
    command_stop(controller);
}
