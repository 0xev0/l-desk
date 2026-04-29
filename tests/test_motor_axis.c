#include "motor_axis.h"
#include "test_support.h"

static void stop_command_deenergizes_all_outputs(void)
{
    motor_axis_output_t output = motor_axis_command_to_output(MOTOR_COMMAND_STOP);

    TEST_ASSERT_FALSE(output.rpwm_active);
    TEST_ASSERT_FALSE(output.lpwm_active);
    TEST_ASSERT_FALSE(output.enable_active);
    TEST_ASSERT_TRUE(motor_axis_output_is_safe(&output));
}

static void up_command_energizes_only_rpwm(void)
{
    motor_axis_output_t output = motor_axis_command_to_output(MOTOR_COMMAND_UP);

    TEST_ASSERT_TRUE(output.rpwm_active);
    TEST_ASSERT_FALSE(output.lpwm_active);
    TEST_ASSERT_TRUE(output.enable_active);
    TEST_ASSERT_TRUE(motor_axis_output_is_safe(&output));
}

static void down_command_energizes_only_lpwm(void)
{
    motor_axis_output_t output = motor_axis_command_to_output(MOTOR_COMMAND_DOWN);

    TEST_ASSERT_FALSE(output.rpwm_active);
    TEST_ASSERT_TRUE(output.lpwm_active);
    TEST_ASSERT_TRUE(output.enable_active);
    TEST_ASSERT_TRUE(motor_axis_output_is_safe(&output));
}

static void invalid_command_fails_safe_to_stop(void)
{
    motor_axis_output_t output = motor_axis_command_to_output((motor_command_t)99);

    TEST_ASSERT_FALSE(output.rpwm_active);
    TEST_ASSERT_FALSE(output.lpwm_active);
    TEST_ASSERT_FALSE(output.enable_active);
    TEST_ASSERT_TRUE(motor_axis_output_is_safe(&output));
}

static void output_with_both_directions_active_is_unsafe(void)
{
    motor_axis_output_t output = {
        .rpwm_active = true,
        .lpwm_active = true,
        .enable_active = true,
    };

    TEST_ASSERT_FALSE(motor_axis_output_is_safe(&output));
}

static void null_output_pointer_is_unsafe(void)
{
    TEST_ASSERT_FALSE(motor_axis_output_is_safe(NULL));
}

int main(void)
{
    stop_command_deenergizes_all_outputs();
    up_command_energizes_only_rpwm();
    down_command_energizes_only_lpwm();
    invalid_command_fails_safe_to_stop();
    output_with_both_directions_active_is_unsafe();
    null_output_pointer_is_unsafe();

    return 0;
}
