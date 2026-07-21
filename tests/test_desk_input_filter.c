#include "desk_input_filter.h"
#include "test_support.h"

static void safety_activation_is_immediate_and_release_is_debounced(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50U);
    desk_inputs_t raw = {0};
    raw.emergency_active = true;
    raw.axis[DESK_AXIS_WING].upper_limit_active = true;
    desk_inputs_t stable = desk_input_filter_update(&filter, &raw, 10U);
    TEST_ASSERT_TRUE(stable.emergency_active);
    TEST_ASSERT_TRUE(stable.axis[DESK_AXIS_WING].upper_limit_active);

    raw.emergency_active = false;
    raw.axis[DESK_AXIS_WING].upper_limit_active = false;
    stable = desk_input_filter_update(&filter, &raw, 40U);
    TEST_ASSERT_TRUE(stable.emergency_active);
    TEST_ASSERT_TRUE(stable.axis[DESK_AXIS_WING].upper_limit_active);
    stable = desk_input_filter_update(&filter, &raw, 10U);
    TEST_ASSERT_FALSE(stable.emergency_active);
    TEST_ASSERT_FALSE(stable.axis[DESK_AXIS_WING].upper_limit_active);
}

static void commands_and_feedback_pass_through(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50U);
    desk_inputs_t raw = {
        .up_pressed = true,
        .calibrate_pressed = true,
    };
    raw.axis[1].hall_count = 1234;
    raw.axis[1].hall_valid = true;
    desk_inputs_t stable = desk_input_filter_update(&filter, &raw, 10U);
    TEST_ASSERT_TRUE(stable.up_pressed);
    TEST_ASSERT_TRUE(stable.calibrate_pressed);
    TEST_ASSERT_EQUAL_INT(1234, stable.axis[1].hall_count);
    TEST_ASSERT_TRUE(stable.axis[1].hall_valid);
}

static void invalid_arguments_fail_safe(void)
{
    desk_inputs_t stable = desk_input_filter_update(NULL, NULL, 10U);
    TEST_ASSERT_TRUE(stable.emergency_active);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_TRUE(stable.axis[axis].upper_limit_active);
        TEST_ASSERT_TRUE(stable.axis[axis].lower_limit_active);
        TEST_ASSERT_FALSE(stable.axis[axis].hall_valid);
    }
}

int main(void)
{
    safety_activation_is_immediate_and_release_is_debounced();
    commands_and_feedback_pass_through();
    invalid_arguments_fail_safe();
    return 0;
}
