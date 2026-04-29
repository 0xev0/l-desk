#include "desk_controller.h"
#include "test_support.h"

#include <limits.h>

static desk_inputs_t no_inputs(void)
{
    return (desk_inputs_t){0};
}

static desk_config_t test_config(void)
{
    return (desk_config_t){
        .control_tick_ms = 10,
        .direction_change_deadtime_ms = 300,
        .alignment_timeout_ms = 3000,
        .calibration_timeout_ms = 45000,
        .button_debounce_ms = 0,
    };
}

static desk_config_t debounce_config(void)
{
    return (desk_config_t){
        .control_tick_ms = 10,
        .direction_change_deadtime_ms = 300,
        .alignment_timeout_ms = 3000,
        .calibration_timeout_ms = 45000,
        .button_debounce_ms = 30,
    };
}

static desk_outputs_t tick_elapsed(desk_controller_t *controller, desk_inputs_t inputs, uint32_t elapsed_ms)
{
    return desk_controller_tick(controller, &inputs, elapsed_ms);
}

static void null_config_uses_safe_defaults(void)
{
    desk_controller_t controller;
    desk_controller_init(&controller, NULL);

    desk_outputs_t outputs = tick_elapsed(&controller, no_inputs(), 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void null_tick_arguments_fail_safe(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);

    desk_outputs_t outputs = desk_controller_tick(NULL, NULL, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);

    outputs = desk_controller_tick(&controller, NULL, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);
}

static void repeated_ticks_with_unchanged_inputs_are_deterministic(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;

    desk_outputs_t first = tick_elapsed(&controller, inputs, 10);
    desk_outputs_t second = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(first.left_motor, second.left_motor);
    TEST_ASSERT_EQUAL_INT(first.right_motor, second.right_motor);
    TEST_ASSERT_EQUAL_INT(first.state, second.state);
    TEST_ASSERT_EQUAL_INT(first.error, second.error);
}

static void large_elapsed_value_during_alignment_fails_safe_without_overflow(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_upper_limit_active = true;
    (void)tick_elapsed(&controller, inputs, 10);

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, UINT_MAX);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, outputs.error);
}

static void active_limit_latches_axis_stopped_until_motion_released(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.left_upper_limit_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);

    inputs.left_upper_limit_active = false;
    outputs = tick_elapsed(&controller, inputs, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);

    inputs.up_pressed = false;
    outputs = tick_elapsed(&controller, inputs, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
}

static void up_button_must_be_stable_before_motion_starts(void)
{
    desk_controller_t controller;
    desk_config_t config = debounce_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);

    outputs = tick_elapsed(&controller, inputs, 19);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);

    outputs = tick_elapsed(&controller, inputs, 1);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
}

static void brief_up_button_bounce_does_not_start_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = debounce_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 20);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);

    inputs.up_pressed = false;
    outputs = tick_elapsed(&controller, inputs, 100);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
}

static void down_button_must_be_stable_before_motion_starts(void)
{
    desk_controller_t controller;
    desk_config_t config = debounce_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 29);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);

    outputs = tick_elapsed(&controller, inputs, 1);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
}

static void release_stops_immediately_even_when_debounce_is_enabled(void)
{
    desk_controller_t controller;
    desk_config_t config = debounce_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_elapsed(&controller, inputs, 30);

    inputs.up_pressed = false;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 1);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
}

int main(void)
{
    null_config_uses_safe_defaults();
    null_tick_arguments_fail_safe();
    repeated_ticks_with_unchanged_inputs_are_deterministic();
    large_elapsed_value_during_alignment_fails_safe_without_overflow();
    active_limit_latches_axis_stopped_until_motion_released();
    up_button_must_be_stable_before_motion_starts();
    brief_up_button_bounce_does_not_start_motion();
    down_button_must_be_stable_before_motion_starts();
    release_stops_immediately_even_when_debounce_is_enabled();

    return 0;
}
