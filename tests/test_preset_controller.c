#include "desk_preset_controller.h"
#include "test_support.h"

#include <limits.h>

static desk_inputs_t positioned_inputs(int32_t position_um)
{
    desk_inputs_t inputs = {0};
    for (size_t axis = 0U; axis < DESK_AXIS_COUNT; ++axis) {
        inputs.axis[axis].hall_valid = true;
        inputs.axis[axis].position_um = position_um;
    }
    return inputs;
}

static void preset_bank_is_fixed_bounded_and_explicit(void)
{
    desk_preset_bank_t bank = {0};
    int32_t target;
    TEST_ASSERT_FALSE(desk_preset_bank_get(&bank, 0U, &target));
    TEST_ASSERT_FALSE(desk_preset_bank_set(NULL, 0U, 10, 100));
    TEST_ASSERT_FALSE(desk_preset_bank_set(&bank, DESK_PRESET_COUNT, 10, 100));
    TEST_ASSERT_FALSE(desk_preset_bank_set(&bank, 0U, -1, 100));
    TEST_ASSERT_FALSE(desk_preset_bank_set(&bank, 0U, 101, 100));
    TEST_ASSERT_FALSE(desk_preset_bank_set(&bank, 0U, 10, 0));
    TEST_ASSERT_TRUE(desk_preset_bank_set(&bank, 0U, 0, 100));
    TEST_ASSERT_TRUE(desk_preset_bank_set(&bank, 1U, 100, 100));
    TEST_ASSERT_TRUE(desk_preset_bank_get(&bank, 1U, &target));
    TEST_ASSERT_EQUAL_INT(100, target);
    TEST_ASSERT_FALSE(desk_preset_bank_get(&bank, DESK_PRESET_COUNT, &target));
    TEST_ASSERT_FALSE(desk_preset_bank_get(&bank, 0U, NULL));
}

static void upward_preset_stops_and_latches_after_crossing(void)
{
    desk_preset_bank_t bank = {0};
    TEST_ASSERT_TRUE(desk_preset_bank_set(&bank, 0U, 200000, 500000));
    desk_preset_controller_t controller = {0};
    desk_inputs_t inputs = positioned_inputs(100000);
    motor_command_t direction;
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_UP, direction);

    inputs = positioned_inputs(199000);
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, direction);
    inputs = positioned_inputs(190000);
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, direction);
}

static void downward_preset_never_reverses_after_crossing(void)
{
    desk_preset_bank_t bank = {0};
    TEST_ASSERT_TRUE(desk_preset_bank_set(&bank, 2U, 150000, 500000));
    desk_preset_controller_t controller = {0};
    desk_inputs_t inputs = positioned_inputs(300000);
    motor_command_t direction;
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 2U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, direction);
    inputs = positioned_inputs(149000);
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 2U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, direction);
    inputs = positioned_inputs(180000);
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 2U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, direction);
}

static void invalid_reference_feedback_and_arguments_fail_stopped(void)
{
    desk_preset_bank_t bank = {0};
    TEST_ASSERT_TRUE(desk_preset_bank_set(&bank, 0U, 100000, 500000));
    desk_preset_controller_t controller = {0};
    desk_inputs_t inputs = positioned_inputs(0);
    motor_command_t direction = MOTOR_COMMAND_UP;
    TEST_ASSERT_FALSE(desk_preset_controller_update(NULL, &bank, 0U, &inputs, true, &direction));
    TEST_ASSERT_FALSE(
        desk_preset_controller_update(&controller, NULL, 0U, &inputs, true, &direction));
    TEST_ASSERT_FALSE(
        desk_preset_controller_update(&controller, &bank, 0U, NULL, true, &direction));
    TEST_ASSERT_FALSE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, false, &direction));
    inputs.axis[1].hall_valid = false;
    TEST_ASSERT_FALSE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, true, &direction));
    desk_preset_controller_reset(&controller);
    TEST_ASSERT_FALSE(controller.active);
    desk_preset_controller_reset(NULL);

    inputs = positioned_inputs(INT32_MAX);
    TEST_ASSERT_TRUE(
        desk_preset_controller_update(&controller, &bank, 0U, &inputs, true, &direction));
    TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_DOWN, direction);
}

static void preset_capture_requires_safe_coherent_feedback(void)
{
    desk_preset_bank_t bank = {0};
    desk_inputs_t inputs = positioned_inputs(200000);
    inputs.axis[0].position_um = 198000;
    inputs.axis[2].position_um = 202000;
    TEST_ASSERT_TRUE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, 5000));
    TEST_ASSERT_EQUAL_INT(200000, bank.target_um[1]);

    TEST_ASSERT_FALSE(desk_preset_bank_capture(NULL, 1U, &inputs, true, false, 500000, 5000));
    TEST_ASSERT_FALSE(
        desk_preset_bank_capture(&bank, DESK_PRESET_COUNT, &inputs, true, false, 500000, 5000));
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, NULL, true, false, 500000, 5000));
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, false, false, 500000, 5000));
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, true, 500000, 5000));
    inputs.axis[2].position_um = 204001;
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, 5000));
    inputs = positioned_inputs(200000);
    inputs.axis[0].hall_valid = false;
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, 5000));
    inputs = positioned_inputs(-1);
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, 5000));
    inputs = positioned_inputs(500001);
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, 5000));
    inputs = positioned_inputs(200000);
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 0, 5000));
    TEST_ASSERT_FALSE(desk_preset_bank_capture(&bank, 1U, &inputs, true, false, 500000, -1));
}

int main(void)
{
    preset_bank_is_fixed_bounded_and_explicit();
    upward_preset_stops_and_latches_after_crossing();
    downward_preset_never_reverses_after_crossing();
    invalid_reference_feedback_and_arguments_fail_stopped();
    preset_capture_requires_safe_coherent_feedback();
    return 0;
}
