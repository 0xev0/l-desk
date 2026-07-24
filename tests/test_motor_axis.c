#include "motor_axis.h"
#include "test_support.h"

static desk_axis_output_t command(motor_command_t direction, uint16_t duty, bool enable)
{
    return (desk_axis_output_t){
        .direction = direction,
        .duty_permille = duty,
        .enable = enable,
    };
}

static void valid_commands_map_to_safe_outputs(void)
{
    desk_axis_output_t stop = command(MOTOR_COMMAND_STOP, 0U, false);
    motor_axis_output_t output = motor_axis_command_to_output(&stop);
    TEST_ASSERT_TRUE(motor_axis_output_is_coherent(&output));
    TEST_ASSERT_FALSE(output.rpwm_active);
    TEST_ASSERT_FALSE(output.lpwm_active);

    desk_axis_output_t up = command(MOTOR_COMMAND_UP, 500U, true);
    output = motor_axis_command_to_output(&up);
    TEST_ASSERT_TRUE(output.rpwm_active);
    TEST_ASSERT_FALSE(output.lpwm_active);
    TEST_ASSERT_EQUAL_INT(500, output.duty_permille);

    desk_axis_output_t down = command(MOTOR_COMMAND_DOWN, 1000U, true);
    output = motor_axis_command_to_output(&down);
    TEST_ASSERT_FALSE(output.rpwm_active);
    TEST_ASSERT_TRUE(output.lpwm_active);
    TEST_ASSERT_TRUE(motor_axis_output_is_safe(&output));
}

static void invalid_commands_fail_safe(void)
{
    const desk_axis_output_t invalid[] = {
        {MOTOR_COMMAND_STOP, 1U, false},
        {MOTOR_COMMAND_STOP, 0U, true},
        {MOTOR_COMMAND_UP, 0U, true},
        {MOTOR_COMMAND_UP, 500U, false},
        {MOTOR_COMMAND_DOWN, 1001U, true},
        {(motor_command_t)99, 500U, true},
    };

    for (size_t item = 0; item < sizeof(invalid) / sizeof(invalid[0]); ++item) {
        TEST_ASSERT_FALSE(motor_axis_command_is_valid(&invalid[item]));
        motor_axis_output_t output = motor_axis_command_to_output(&invalid[item]);
        TEST_ASSERT_TRUE(motor_axis_output_is_coherent(&output));
    }
    TEST_ASSERT_FALSE(motor_axis_command_is_valid(NULL));
}

static void physical_output_checks_reject_unsafe_and_incoherent_states(void)
{
    TEST_ASSERT_FALSE(motor_axis_output_is_safe(NULL));
    TEST_ASSERT_FALSE(motor_axis_output_is_safe(&(motor_axis_output_t){
        .rpwm_active = true,
        .lpwm_active = true,
        .enable_active = true,
        .duty_permille = 500U,
    }));
    TEST_ASSERT_FALSE(motor_axis_output_is_safe(&(motor_axis_output_t){
        .rpwm_active = true,
        .enable_active = true,
        .duty_permille = 1001U,
    }));

    TEST_ASSERT_TRUE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .rpwm_active = true,
        .enable_active = true,
        .duty_permille = 500U,
    }));
    TEST_ASSERT_TRUE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .lpwm_active = true,
        .enable_active = true,
        .duty_permille = 500U,
    }));
    TEST_ASSERT_FALSE(motor_axis_output_is_coherent(NULL));
    TEST_ASSERT_FALSE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .rpwm_active = true,
        .lpwm_active = true,
        .enable_active = true,
        .duty_permille = 500U,
    }));
    TEST_ASSERT_FALSE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .enable_active = true,
    }));
    TEST_ASSERT_FALSE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .rpwm_active = true,
        .duty_permille = 500U,
    }));
    TEST_ASSERT_FALSE(motor_axis_output_is_coherent(&(motor_axis_output_t){
        .rpwm_active = true,
        .enable_active = true,
    }));
}

static void complete_output_vector_is_checked_before_hardware(void)
{
    desk_outputs_t outputs = {
        .state = DESK_STATE_MOVING_UP,
        .error = DESK_ERROR_NONE,
    };
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = command(MOTOR_COMMAND_UP, 500U, true);
    }
    TEST_ASSERT_TRUE(desk_outputs_are_coherent(&outputs));

    outputs.motor[DESK_AXIS_WING] = command(MOTOR_COMMAND_DOWN, 500U, true);
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(&outputs));
    outputs.state = DESK_STATE_ERROR;
    outputs.error = DESK_ERROR_HARDWARE_FAULT;
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(&outputs));

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = command(MOTOR_COMMAND_STOP, 0U, false);
    }
    TEST_ASSERT_TRUE(desk_outputs_are_coherent(&outputs));
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(NULL));

    outputs.state = (desk_state_t)(DESK_STATE_ERROR + 1);
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(&outputs));
    outputs = (desk_outputs_t){
        .state = DESK_STATE_UNREFERENCED,
        .error = DESK_ERROR_NONE,
    };
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = command(MOTOR_COMMAND_STOP, 0U, false);
    }
    TEST_ASSERT_TRUE(desk_outputs_are_coherent(&outputs));

    outputs.state = DESK_STATE_MOVING_DOWN;
    outputs.motor[DESK_AXIS_FRONT_LEFT] = command(MOTOR_COMMAND_UP, 500U, true);
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(&outputs));
    outputs.state = DESK_STATE_CALIBRATING_DOWN;
    TEST_ASSERT_FALSE(desk_outputs_are_coherent(&outputs));

    outputs.state = DESK_STATE_MOVING_DOWN;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        outputs.motor[axis] = command(MOTOR_COMMAND_DOWN, 500U, true);
    }
    TEST_ASSERT_TRUE(desk_outputs_are_coherent(&outputs));
}

int main(void)
{
    valid_commands_map_to_safe_outputs();
    invalid_commands_fail_safe();
    physical_output_checks_reject_unsafe_and_incoherent_states();
    complete_output_vector_is_checked_before_hardware();
    return 0;
}
