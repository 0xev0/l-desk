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

static desk_config_t short_calibration_config(void)
{
    return (desk_config_t){
        .control_tick_ms = 10,
        .direction_change_deadtime_ms = 300,
        .alignment_timeout_ms = 3000,
        .calibration_timeout_ms = 100,
    };
}

static desk_outputs_t tick_elapsed(desk_controller_t *controller, desk_inputs_t inputs, uint32_t elapsed_ms)
{
    return desk_controller_tick(controller, &inputs, elapsed_ms);
}

static void calibration_starts_both_motors_down(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_CALIBRATING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void calibration_stops_axis_that_reaches_lower_limit_first(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.calibrate_pressed = false;
    inputs.left_lower_limit_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_CALIBRATING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void calibration_succeeds_when_both_lower_limits_are_reached(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.calibrate_pressed = false;
    inputs.left_lower_limit_active = true;
    (void)tick_elapsed(&controller, inputs, 10);
    inputs.right_lower_limit_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 1000);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void calibration_with_both_lower_limits_already_active_succeeds_without_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;
    inputs.left_lower_limit_active = true;
    inputs.right_lower_limit_active = true;

    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void calibration_times_out_if_lower_limits_are_not_reached(void)
{
    desk_controller_t controller;
    desk_config_t config = short_calibration_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.calibrate_pressed = false;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 100);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, outputs.error);
}

static void calibration_aborts_on_emergency_stop(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.calibrate_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.emergency_active = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, outputs.error);
}

int main(void)
{
    calibration_starts_both_motors_down();
    calibration_stops_axis_that_reaches_lower_limit_first();
    calibration_succeeds_when_both_lower_limits_are_reached();
    calibration_with_both_lower_limits_already_active_succeeds_without_motion();
    calibration_times_out_if_lower_limits_are_not_reached();
    calibration_aborts_on_emergency_stop();

    return 0;
}
