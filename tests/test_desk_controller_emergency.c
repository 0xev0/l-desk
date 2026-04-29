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

static desk_outputs_t tick_once(desk_controller_t *controller, desk_inputs_t inputs)
{
    return desk_controller_tick(controller, &inputs, 10);
}

static void emergency_active_in_idle_enters_error_with_motors_stopped(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.emergency_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, outputs.error);
}

static void emergency_active_while_moving_up_stops_immediately(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_once(&controller, inputs);

    inputs.emergency_active = true;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, outputs.error);
}

static void emergency_active_while_moving_down_stops_immediately(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    (void)tick_once(&controller, inputs);

    inputs.emergency_active = true;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, outputs.error);
}

static void emergency_release_does_not_clear_error_automatically(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.emergency_active = true;
    (void)tick_once(&controller, inputs);

    inputs.emergency_active = false;
    inputs.up_pressed = true;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, outputs.error);
}

static void clear_error_requires_safe_released_inputs_and_does_not_move(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.emergency_active = true;
    (void)tick_once(&controller, inputs);

    desk_controller_request_clear_error(&controller);
    inputs.emergency_active = false;
    inputs.up_pressed = false;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

int main(void)
{
    emergency_active_in_idle_enters_error_with_motors_stopped();
    emergency_active_while_moving_up_stops_immediately();
    emergency_active_while_moving_down_stops_immediately();
    emergency_release_does_not_clear_error_automatically();
    clear_error_requires_safe_released_inputs_and_does_not_move();

    return 0;
}
