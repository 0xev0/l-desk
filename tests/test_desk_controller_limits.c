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

static void left_upper_limit_blocks_only_left_upward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_upper_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void right_upper_limit_blocks_only_right_upward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.right_upper_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void lower_limits_do_not_block_upward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_lower_limit_active = true;
    inputs.right_lower_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void left_lower_limit_blocks_only_left_downward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    inputs.left_lower_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void right_lower_limit_blocks_only_right_downward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    inputs.right_lower_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void upper_limits_do_not_block_downward_motion(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    inputs.left_upper_limit_active = true;
    inputs.right_upper_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

static void both_upper_and_lower_active_on_left_axis_enters_error(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.left_upper_limit_active = true;
    inputs.left_lower_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_LIMIT_CONFLICT, outputs.error);
}

static void both_upper_and_lower_active_on_right_axis_enters_error(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    inputs.right_upper_limit_active = true;
    inputs.right_lower_limit_active = true;

    desk_outputs_t outputs = tick_once(&controller, inputs);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_LIMIT_CONFLICT, outputs.error);
}

int main(void)
{
    left_upper_limit_blocks_only_left_upward_motion();
    right_upper_limit_blocks_only_right_upward_motion();
    lower_limits_do_not_block_upward_motion();
    left_lower_limit_blocks_only_left_downward_motion();
    right_lower_limit_blocks_only_right_downward_motion();
    upper_limits_do_not_block_downward_motion();
    both_upper_and_lower_active_on_left_axis_enters_error();
    both_upper_and_lower_active_on_right_axis_enters_error();

    return 0;
}
