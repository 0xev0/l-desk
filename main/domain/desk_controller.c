#include "desk_controller.h"

#include <limits.h>
#include <stddef.h>

desk_config_t desk_config_default(void)
{
    return (desk_config_t){
        .control_tick_ms = 10U,
        .max_control_interval_ms = 50U,
        .safe_start_release_ms = 500U,
        .direction_change_deadtime_ms = 300U,
        .alignment_timeout_ms = 750U,
        .calibration_timeout_ms = 45000U,
        .button_debounce_ms = 50U,
        .calibrate_hold_ms = 2000U,
        .soft_start_ms = 500U,
        .feedback_stall_timeout_ms = 1000U,
        .feedback_warning_counts = 30,
        .feedback_stop_counts = 50,
        .maximum_duty_permille = 1000U,
        .starting_duty_permille = 300U,
        .synchronization_trim_permille = 200U,
        .feedback_required = true,
    };
}

static desk_axis_output_t stopped_axis(void)
{
    return (desk_axis_output_t){
        .direction = MOTOR_COMMAND_STOP,
        .duty_permille = 0U,
        .enable = false,
    };
}

static bool any_motor_enabled(const desk_controller_t *controller)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (controller->motor[axis].enable) {
            return true;
        }
    }

    return false;
}

static void stop_all(desk_controller_t *controller, uint64_t now_ms)
{
    bool was_moving = any_motor_enabled(controller);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        controller->motor[axis] = stopped_axis();
    }

    if (was_moving) {
        controller->direction_stopped_at_ms = now_ms;
    }
    controller->motion_was_active = false;
}

static desk_outputs_t current_outputs(const desk_controller_t *controller)
{
    desk_outputs_t outputs = {
        .state = controller->state,
        .error = controller->error,
        .feedback_warning = controller->feedback_warning,
    };

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = controller->motor[axis];
    }
    return outputs;
}

static desk_outputs_t fault_outputs(desk_error_t error)
{
    desk_outputs_t outputs = {
        .state = DESK_STATE_ERROR,
        .error = error,
    };

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = stopped_axis();
    }
    return outputs;
}

static void reset_alignment(desk_controller_t *controller)
{
    controller->alignment_active = false;
    controller->alignment_started_at_ms = 0U;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        controller->axis_at_endstop[axis] = false;
    }
}

static void reset_button_debounce(desk_controller_t *controller)
{
    controller->debounce_candidate_direction = MOTOR_COMMAND_STOP;
    controller->button_candidate_since_ms = 0U;
}

static void enter_error(desk_controller_t *controller, desk_error_t error, uint64_t now_ms)
{
    stop_all(controller, now_ms);
    controller->state = DESK_STATE_ERROR;
    controller->error = error;
    controller->pending_direction = MOTOR_COMMAND_STOP;
    controller->calibrate_timing = false;
}

static bool has_limit_conflict(const desk_inputs_t *inputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (inputs->axis[axis].upper_limit_active && inputs->axis[axis].lower_limit_active) {
            return true;
        }
    }

    return false;
}

static bool all_feedback_valid(const desk_inputs_t *inputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (!inputs->axis[axis].hall_valid) {
            return false;
        }
    }

    return true;
}

static bool controls_released(const desk_inputs_t *inputs)
{
    return !inputs->up_pressed && !inputs->down_pressed && !inputs->calibrate_pressed;
}

static bool safe_start_inputs(const desk_controller_t *controller, const desk_inputs_t *inputs)
{
    return controls_released(inputs) && !inputs->emergency_active && !has_limit_conflict(inputs) &&
           (!controller->config.feedback_required || all_feedback_valid(inputs));
}

static bool matching_limit_active(const desk_axis_inputs_t *axis, motor_command_t direction)
{
    if (direction == MOTOR_COMMAND_UP) {
        return axis->upper_limit_active;
    }
    if (direction == MOTOR_COMMAND_DOWN) {
        return axis->lower_limit_active;
    }
    return false;
}

static motor_command_t requested_manual_direction(const desk_inputs_t *inputs)
{
    if (inputs->up_pressed && !inputs->down_pressed) {
        return MOTOR_COMMAND_UP;
    }
    if (inputs->down_pressed && !inputs->up_pressed) {
        return MOTOR_COMMAND_DOWN;
    }
    return MOTOR_COMMAND_STOP;
}

static bool is_direction_reversal(motor_command_t previous, motor_command_t requested)
{
    return (previous == MOTOR_COMMAND_UP && requested == MOTOR_COMMAND_DOWN) ||
           (previous == MOTOR_COMMAND_DOWN && requested == MOTOR_COMMAND_UP);
}

static bool config_is_valid(const desk_config_t *config)
{
    return config->control_tick_ms > 0U &&
           config->max_control_interval_ms >= config->control_tick_ms &&
           config->alignment_timeout_ms > 0U && config->calibration_timeout_ms > 0U &&
           (!config->feedback_required || config->feedback_stall_timeout_ms > 0U) &&
           config->feedback_warning_counts >= 0 &&
           config->feedback_stop_counts > config->feedback_warning_counts &&
           config->maximum_duty_permille > 0U && config->maximum_duty_permille <= 1000U &&
           config->starting_duty_permille > 0U &&
           config->starting_duty_permille <= config->maximum_duty_permille &&
           config->synchronization_trim_permille < config->maximum_duty_permille;
}

static uint64_t saturating_deadline(uint64_t now_ms, uint32_t delay_ms)
{
    return UINT64_MAX - now_ms < delay_ms ? UINT64_MAX : now_ms + delay_ms;
}

static motor_command_t debounce_direction(desk_controller_t *controller,
                                          motor_command_t requested,
                                          uint64_t now_ms)
{
    if (requested == MOTOR_COMMAND_STOP) {
        reset_button_debounce(controller);
        return MOTOR_COMMAND_STOP;
    }

    if (controller->config.button_debounce_ms == 0U) {
        return requested;
    }

    if (controller->debounce_candidate_direction != requested) {
        controller->debounce_candidate_direction = requested;
        controller->button_candidate_since_ms = now_ms;
        return MOTOR_COMMAND_STOP;
    }

    if (now_ms - controller->button_candidate_since_ms < controller->config.button_debounce_ms) {
        return MOTOR_COMMAND_STOP;
    }

    return requested;
}

static uint16_t ramped_duty(const desk_controller_t *controller, uint64_t now_ms)
{
    uint16_t maximum = controller->config.maximum_duty_permille;
    uint16_t starting = controller->config.starting_duty_permille;
    if (maximum > 1000U) {
        maximum = 1000U;
    }
    if (starting > maximum) {
        starting = maximum;
    }
    if (controller->config.soft_start_ms == 0U || now_ms <= controller->motion_started_at_ms) {
        return controller->config.soft_start_ms == 0U ? maximum : starting;
    }

    uint64_t elapsed = now_ms - controller->motion_started_at_ms;
    if (elapsed >= controller->config.soft_start_ms) {
        return maximum;
    }

    uint32_t range = (uint32_t)maximum - starting;
    return (uint16_t)(starting + (range * elapsed) / controller->config.soft_start_ms);
}

static uint32_t feedback_spread(const desk_inputs_t *inputs, int32_t *minimum, int32_t *maximum)
{
    int32_t min_value = inputs->axis[0].hall_count;
    int32_t max_value = min_value;

    for (size_t axis = 1; axis < DESK_AXIS_COUNT; ++axis) {
        if (inputs->axis[axis].hall_count < min_value) {
            min_value = inputs->axis[axis].hall_count;
        }
        if (inputs->axis[axis].hall_count > max_value) {
            max_value = inputs->axis[axis].hall_count;
        }
    }

    *minimum = min_value;
    *maximum = max_value;
    return (uint32_t)((int64_t)max_value - min_value);
}

static bool validate_previous_feedback(desk_controller_t *controller,
                                       const desk_inputs_t *inputs,
                                       uint64_t now_ms)
{
    if (!controller->config.feedback_required || !controller->motion_was_active) {
        return true;
    }

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (!controller->motor[axis].enable) {
            continue;
        }
        if (!inputs->axis[axis].hall_valid) {
            enter_error(controller, DESK_ERROR_FEEDBACK_MISSING, now_ms);
            return false;
        }

        int64_t delta = (int64_t)inputs->axis[axis].hall_count - controller->previous_hall_count[axis];
        if ((controller->motor[axis].direction == MOTOR_COMMAND_UP && delta < 0) ||
            (controller->motor[axis].direction == MOTOR_COMMAND_DOWN && delta > 0)) {
            enter_error(controller, DESK_ERROR_FEEDBACK_DIRECTION, now_ms);
            return false;
        }

        if (delta != 0) {
            controller->feedback_changed_at_ms[axis] = now_ms;
        } else if (controller->config.feedback_stall_timeout_ms > 0U &&
                   now_ms - controller->feedback_changed_at_ms[axis] >=
                       controller->config.feedback_stall_timeout_ms) {
            enter_error(controller, DESK_ERROR_FEEDBACK_STALL, now_ms);
            return false;
        }
    }

    return true;
}

static void capture_feedback(desk_controller_t *controller, const desk_inputs_t *inputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        controller->previous_hall_count[axis] = inputs->axis[axis].hall_count;
    }
    controller->feedback_initialized = true;
}

static bool update_endstop_alignment(desk_controller_t *controller,
                                     const desk_inputs_t *inputs,
                                     motor_command_t direction,
                                     uint64_t now_ms)
{
    size_t stopped_count = 0U;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (matching_limit_active(&inputs->axis[axis], direction)) {
            controller->axis_at_endstop[axis] = true;
        }
        if (controller->axis_at_endstop[axis]) {
            ++stopped_count;
        }
    }

    if (stopped_count == DESK_AXIS_COUNT) {
        stop_all(controller, now_ms);
        controller->state = DESK_STATE_IDLE;
        controller->error = DESK_ERROR_NONE;
        reset_alignment(controller);
        return false;
    }

    if (stopped_count > 0U) {
        if (!controller->alignment_active) {
            controller->alignment_active = true;
            controller->alignment_started_at_ms = now_ms;
        } else if (now_ms - controller->alignment_started_at_ms >=
                   controller->config.alignment_timeout_ms) {
            enter_error(controller, DESK_ERROR_ALIGNMENT_TIMEOUT, now_ms);
            return false;
        }
    }

    return true;
}

static bool apply_feedback_synchronization(desk_controller_t *controller,
                                           const desk_inputs_t *inputs,
                                           motor_command_t direction,
                                           uint16_t base_duty,
                                           uint64_t now_ms,
                                           uint16_t duty[DESK_AXIS_COUNT])
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        duty[axis] = base_duty;
    }
    controller->feedback_warning = false;

    if (!controller->config.feedback_required) {
        return true;
    }
    if (!all_feedback_valid(inputs)) {
        enter_error(controller, DESK_ERROR_FEEDBACK_MISSING, now_ms);
        return false;
    }

    int32_t minimum = 0;
    int32_t maximum = 0;
    uint32_t spread = feedback_spread(inputs, &minimum, &maximum);
    if (spread > (uint32_t)controller->config.feedback_stop_counts) {
        enter_error(controller, DESK_ERROR_FEEDBACK_SKEW, now_ms);
        return false;
    }
    if (spread < (uint32_t)controller->config.feedback_warning_counts) {
        return true;
    }

    controller->feedback_warning = true;

    uint16_t trim = controller->config.synchronization_trim_permille;
    uint16_t reduced = base_duty > trim ? (uint16_t)(base_duty - trim) : 1U;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        bool leading_up = direction == MOTOR_COMMAND_UP && inputs->axis[axis].hall_count == maximum;
        bool leading_down = direction == MOTOR_COMMAND_DOWN && inputs->axis[axis].hall_count == minimum;
        if (leading_up || leading_down) {
            duty[axis] = reduced;
        }
    }
    return true;
}

static void command_direction(desk_controller_t *controller,
                              const desk_inputs_t *inputs,
                              motor_command_t direction,
                              uint64_t now_ms)
{
    if (!any_motor_enabled(controller) || controller->last_motion_direction != direction) {
        controller->motion_started_at_ms = now_ms;
        for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
            controller->feedback_changed_at_ms[axis] = now_ms;
        }
    }

    if (!update_endstop_alignment(controller, inputs, direction, now_ms) ||
        controller->state == DESK_STATE_ERROR) {
        return;
    }

    uint16_t duty[DESK_AXIS_COUNT];
    if (!apply_feedback_synchronization(controller,
                                        inputs,
                                        direction,
                                        ramped_duty(controller, now_ms),
                                        now_ms,
                                        duty)) {
        return;
    }

    bool moving = false;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (controller->axis_at_endstop[axis]) {
            controller->motor[axis] = stopped_axis();
        } else {
            controller->motor[axis] = (desk_axis_output_t){
                .direction = direction,
                .duty_permille = duty[axis],
                .enable = true,
            };
            moving = true;
        }
    }

    if (moving) {
        controller->state = direction == MOTOR_COMMAND_UP ? DESK_STATE_MOVING_UP
                                                          : DESK_STATE_MOVING_DOWN;
        controller->last_motion_direction = direction;
        controller->motion_was_active = true;
    }
}

static void handle_safe_start(desk_controller_t *controller,
                              const desk_inputs_t *inputs,
                              uint64_t now_ms)
{
    stop_all(controller, now_ms);
    if (!safe_start_inputs(controller, inputs)) {
        controller->safe_start_timing = false;
        return;
    }

    if (!controller->safe_start_timing) {
        controller->safe_start_timing = true;
        controller->safe_start_since_ms = now_ms;
        return;
    }

    if (now_ms - controller->safe_start_since_ms >= controller->config.safe_start_release_ms) {
        controller->state = DESK_STATE_IDLE;
        controller->safe_start_timing = false;
    }
}

static void handle_calibration(desk_controller_t *controller,
                               const desk_inputs_t *inputs,
                               uint64_t now_ms)
{
    if (!inputs->calibrate_pressed) {
        stop_all(controller, now_ms);
        controller->state = DESK_STATE_IDLE;
        controller->error = DESK_ERROR_NONE;
        controller->calibrate_timing = false;
        reset_alignment(controller);
        return;
    }

    if (now_ms - controller->calibration_started_at_ms >= controller->config.calibration_timeout_ms) {
        enter_error(controller, DESK_ERROR_CALIBRATION_TIMEOUT, now_ms);
        return;
    }

    command_direction(controller, inputs, MOTOR_COMMAND_DOWN, now_ms);
    if (controller->state == DESK_STATE_MOVING_DOWN) {
        controller->state = DESK_STATE_CALIBRATING_DOWN;
    }
}

static bool validate_time(desk_controller_t *controller, uint64_t now_ms)
{
    if (!controller->tick_initialized) {
        controller->tick_initialized = true;
        controller->last_tick_ms = now_ms;
        return true;
    }

    if (now_ms < controller->last_tick_ms ||
        now_ms - controller->last_tick_ms > controller->config.max_control_interval_ms) {
        enter_error(controller, DESK_ERROR_CONTROL_LOOP_OVERRUN, now_ms);
        return false;
    }

    controller->last_tick_ms = now_ms;
    return true;
}

void desk_controller_init(desk_controller_t *controller, const desk_config_t *config)
{
    if (controller == NULL) {
        return;
    }

    desk_config_t resolved_config = config != NULL ? *config : desk_config_default();
    *controller = (desk_controller_t){
        .config = resolved_config,
        .state = DESK_STATE_SAFE_START,
        .error = DESK_ERROR_NONE,
        .last_motion_direction = MOTOR_COMMAND_STOP,
        .pending_direction = MOTOR_COMMAND_STOP,
        .debounce_candidate_direction = MOTOR_COMMAND_STOP,
    };
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        controller->motor[axis] = stopped_axis();
    }
    if (!config_is_valid(&resolved_config)) {
        controller->state = DESK_STATE_ERROR;
        controller->error = DESK_ERROR_HARDWARE_FAULT;
    }
}

desk_outputs_t desk_controller_tick(desk_controller_t *controller,
                                    const desk_inputs_t *inputs,
                                    uint64_t now_ms)
{
    if (controller == NULL || inputs == NULL) {
        return fault_outputs(DESK_ERROR_HARDWARE_FAULT);
    }
    if (!validate_time(controller, now_ms)) {
        return current_outputs(controller);
    }
    if (controller->state == DESK_STATE_ERROR) {
        stop_all(controller, now_ms);
        return current_outputs(controller);
    }
    if (inputs->hardware_fault) {
        enter_error(controller, DESK_ERROR_HARDWARE_FAULT, now_ms);
        return current_outputs(controller);
    }
    if (inputs->emergency_active) {
        enter_error(controller, DESK_ERROR_EMERGENCY_STOP, now_ms);
        return current_outputs(controller);
    }
    if (has_limit_conflict(inputs)) {
        enter_error(controller, DESK_ERROR_LIMIT_CONFLICT, now_ms);
        return current_outputs(controller);
    }
    if (inputs->up_pressed && inputs->down_pressed) {
        enter_error(controller, DESK_ERROR_BOTH_BUTTONS_PRESSED, now_ms);
        return current_outputs(controller);
    }

    if (!controller->feedback_initialized) {
        capture_feedback(controller, inputs);
    } else if (!validate_previous_feedback(controller, inputs, now_ms)) {
        capture_feedback(controller, inputs);
        return current_outputs(controller);
    }
    capture_feedback(controller, inputs);

    if (controller->state == DESK_STATE_SAFE_START) {
        handle_safe_start(controller, inputs, now_ms);
        return current_outputs(controller);
    }
    if (controller->state == DESK_STATE_CALIBRATING_DOWN) {
        handle_calibration(controller, inputs, now_ms);
        return current_outputs(controller);
    }

    if (inputs->calibrate_pressed) {
        if (inputs->up_pressed || inputs->down_pressed || any_motor_enabled(controller)) {
            enter_error(controller, DESK_ERROR_INVALID_TRANSITION, now_ms);
            return current_outputs(controller);
        }
        stop_all(controller, now_ms);
        if (!controller->calibrate_timing) {
            controller->calibrate_timing = true;
            controller->calibrate_pressed_since_ms = now_ms;
        } else if (now_ms - controller->calibrate_pressed_since_ms >=
                   controller->config.calibrate_hold_ms) {
            controller->calibration_started_at_ms = now_ms;
            controller->state = DESK_STATE_CALIBRATING_DOWN;
            reset_alignment(controller);
            handle_calibration(controller, inputs, now_ms);
        }
        return current_outputs(controller);
    }
    controller->calibrate_timing = false;

    motor_command_t raw_requested = requested_manual_direction(inputs);
    if (raw_requested == MOTOR_COMMAND_STOP) {
        stop_all(controller, now_ms);
        controller->state = DESK_STATE_IDLE;
        controller->error = DESK_ERROR_NONE;
        controller->pending_direction = MOTOR_COMMAND_STOP;
        reset_button_debounce(controller);
        reset_alignment(controller);
        return current_outputs(controller);
    }

    motor_command_t requested = debounce_direction(controller, raw_requested, now_ms);
    if (is_direction_reversal(controller->last_motion_direction, raw_requested)) {
        if (any_motor_enabled(controller)) {
            stop_all(controller, now_ms);
            controller->pending_direction = raw_requested;
            controller->deadtime_until_ms = saturating_deadline(
                now_ms,
                controller->config.direction_change_deadtime_ms);
        } else if (controller->pending_direction != raw_requested) {
            controller->pending_direction = raw_requested;
            uint64_t elapsed_stopped = now_ms - controller->direction_stopped_at_ms;
            uint64_t remaining = elapsed_stopped >= controller->config.direction_change_deadtime_ms
                                     ? 0U
                                     : controller->config.direction_change_deadtime_ms - elapsed_stopped;
            controller->deadtime_until_ms = saturating_deadline(now_ms, (uint32_t)remaining);
        }

        if (now_ms < controller->deadtime_until_ms) {
            controller->state = DESK_STATE_IDLE;
            stop_all(controller, now_ms);
            return current_outputs(controller);
        }
        controller->pending_direction = MOTOR_COMMAND_STOP;
    }

    if (requested == MOTOR_COMMAND_STOP) {
        stop_all(controller, now_ms);
        controller->state = DESK_STATE_IDLE;
        return current_outputs(controller);
    }

    command_direction(controller, inputs, requested, now_ms);
    return current_outputs(controller);
}

bool desk_controller_request_clear_error(desk_controller_t *controller,
                                         const desk_inputs_t *inputs,
                                         uint64_t now_ms)
{
    if (controller == NULL || inputs == NULL || controller->state != DESK_STATE_ERROR ||
        !safe_start_inputs(controller, inputs)) {
        return false;
    }

    stop_all(controller, now_ms);
    controller->state = DESK_STATE_SAFE_START;
    controller->error = DESK_ERROR_NONE;
    controller->last_motion_direction = MOTOR_COMMAND_STOP;
    controller->pending_direction = MOTOR_COMMAND_STOP;
    controller->tick_initialized = false;
    controller->feedback_initialized = false;
    controller->safe_start_timing = false;
    controller->calibrate_timing = false;
    reset_button_debounce(controller);
    reset_alignment(controller);
    return true;
}

void desk_controller_report_hardware_fault(desk_controller_t *controller, uint64_t now_ms)
{
    if (controller != NULL) {
        enter_error(controller, DESK_ERROR_HARDWARE_FAULT, now_ms);
    }
}
