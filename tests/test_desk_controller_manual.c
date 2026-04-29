#include "desk_controller.h"
#include "test_support.h"

static desk_inputs_t no_inputs(void)
{
    return (desk_inputs_t){
        .up_pressed = false,
        .down_pressed = false,
        .calibrate_pressed = false,
        .emergency_active = false,
        .left_upper_limit_active = false,
        .left_lower_limit_active = false,
        .right_upper_limit_active = false,
        .right_lower_limit_active = false,
    };
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

static void boot_starts_idle_with_motors_stopped(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);

    desk_outputs_t outputs = tick_once(&controller, no_inputs());

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void pressing_up_moves_both_motors_up(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void releasing_up_stops_both_motors(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_once(&controller, inputs);

    inputs.up_pressed = false;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void pressing_down_moves_both_motors_down(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void releasing_down_stops_both_motors(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    (void)tick_once(&controller, inputs);

    inputs.down_pressed = false;
    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void pressing_up_and_down_stops_and_records_error(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.down_pressed = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_BOTH_BUTTONS_PRESSED, outputs.error);
}

int main(void)
{
    boot_starts_idle_with_motors_stopped();
    pressing_up_moves_both_motors_up();
    releasing_up_stops_both_motors();
    pressing_down_moves_both_motors_down();
    releasing_down_stops_both_motors();
    pressing_up_and_down_stops_and_records_error();

    return 0;
}
