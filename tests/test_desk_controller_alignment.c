#include "desk_controller.h"
#include "test_support.h"

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
    };
}

static desk_outputs_t tick_elapsed(desk_controller_t *controller, desk_inputs_t inputs, uint32_t elapsed_ms)
{
    return desk_controller_tick(controller, &inputs, elapsed_ms);
}

static void moving_up_times_out_if_right_never_reaches_upper_limit(void)
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
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);

    outputs = tick_elapsed(&controller, inputs, 2999);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);

    outputs = tick_elapsed(&controller, inputs, 1);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, outputs.error);
}

static void moving_down_times_out_if_left_never_reaches_lower_limit(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.right_lower_limit_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);

    outputs = tick_elapsed(&controller, inputs, 3000);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, outputs.error);
}

static void both_upper_limits_within_alignment_timeout_stop_and_return_idle(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_upper_limit_active = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.right_upper_limit_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 1000);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void alignment_timer_resets_after_motion_is_released(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_upper_limit_active = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.up_pressed = false;
    (void)tick_elapsed(&controller, inputs, 5000);

    inputs.up_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 2999);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);

    outputs = tick_elapsed(&controller, inputs, 1);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, outputs.error);
}

int main(void)
{
    moving_up_times_out_if_right_never_reaches_upper_limit();
    moving_down_times_out_if_left_never_reaches_lower_limit();
    both_upper_limits_within_alignment_timeout_stop_and_return_idle();
    alignment_timer_resets_after_motion_is_released();

    return 0;
}
