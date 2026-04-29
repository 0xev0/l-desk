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

static void reversing_from_up_to_down_stops_during_deadtime_then_resumes(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.up_pressed = false;
    inputs.down_pressed = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);

    outputs = tick_elapsed(&controller, inputs, 299);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);

    outputs = tick_elapsed(&controller, inputs, 1);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, outputs.state);
}

static void reversing_from_down_to_up_stops_during_deadtime_then_resumes(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.down_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.down_pressed = false;
    inputs.up_pressed = true;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 10);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);

    outputs = tick_elapsed(&controller, inputs, 300);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, outputs.state);
}

static void releasing_button_during_deadtime_cancels_pending_reversal(void)
{
    desk_controller_t controller;
    desk_config_t config = test_config();
    desk_controller_init(&controller, &config);
    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.up_pressed = false;
    inputs.down_pressed = true;
    (void)tick_elapsed(&controller, inputs, 10);

    inputs.down_pressed = false;
    desk_outputs_t outputs = tick_elapsed(&controller, inputs, 500);

    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.left_motor);
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.right_motor);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_NONE, outputs.error);
}

int main(void)
{
    reversing_from_up_to_down_stops_during_deadtime_then_resumes();
    reversing_from_down_to_up_stops_during_deadtime_then_resumes();
    releasing_button_during_deadtime_cancels_pending_reversal();

    return 0;
}
