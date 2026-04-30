#include "desk_input_filter.h"
#include "test_support.h"

static desk_inputs_t no_inputs(void)
{
    return (desk_inputs_t){0};
}

static desk_inputs_t filter_update(desk_input_filter_t *filter, desk_inputs_t inputs, uint32_t elapsed_ms)
{
    return desk_input_filter_update(filter, &inputs, elapsed_ms);
}

static void emergency_activates_immediately(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50);

    desk_inputs_t inputs = no_inputs();
    inputs.emergency_active = true;

    desk_inputs_t filtered = filter_update(&filter, inputs, 1);

    TEST_ASSERT_TRUE(filtered.emergency_active);
}

static void emergency_clear_is_debounced(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50);

    desk_inputs_t inputs = no_inputs();
    inputs.emergency_active = true;
    (void)filter_update(&filter, inputs, 1);

    inputs.emergency_active = false;
    desk_inputs_t filtered = filter_update(&filter, inputs, 49);
    TEST_ASSERT_TRUE(filtered.emergency_active);

    filtered = filter_update(&filter, inputs, 1);
    TEST_ASSERT_FALSE(filtered.emergency_active);
}

static void limit_clear_is_debounced(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50);

    desk_inputs_t inputs = no_inputs();
    inputs.left_upper_limit_active = true;
    desk_inputs_t filtered = filter_update(&filter, inputs, 1);
    TEST_ASSERT_TRUE(filtered.left_upper_limit_active);

    inputs.left_upper_limit_active = false;
    filtered = filter_update(&filter, inputs, 20);
    TEST_ASSERT_TRUE(filtered.left_upper_limit_active);

    inputs.left_upper_limit_active = true;
    filtered = filter_update(&filter, inputs, 1);
    TEST_ASSERT_TRUE(filtered.left_upper_limit_active);

    inputs.left_upper_limit_active = false;
    filtered = filter_update(&filter, inputs, 50);
    TEST_ASSERT_FALSE(filtered.left_upper_limit_active);
}

static void buttons_pass_through_without_filter_delay(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50);

    desk_inputs_t inputs = no_inputs();
    inputs.up_pressed = true;
    inputs.calibrate_pressed = true;

    desk_inputs_t filtered = filter_update(&filter, inputs, 1);

    TEST_ASSERT_TRUE(filtered.up_pressed);
    TEST_ASSERT_TRUE(filtered.calibrate_pressed);

    inputs.up_pressed = false;
    inputs.down_pressed = true;
    inputs.calibrate_pressed = false;
    filtered = filter_update(&filter, inputs, 1);

    TEST_ASSERT_FALSE(filtered.up_pressed);
    TEST_ASSERT_TRUE(filtered.down_pressed);
    TEST_ASSERT_FALSE(filtered.calibrate_pressed);
}

static void null_arguments_fail_safe(void)
{
    desk_input_filter_t filter;
    desk_input_filter_init(&filter, 50);

    desk_inputs_t filtered = desk_input_filter_update(NULL, NULL, 1);
    TEST_ASSERT_TRUE(filtered.emergency_active);
    TEST_ASSERT_TRUE(filtered.left_upper_limit_active);
    TEST_ASSERT_TRUE(filtered.left_lower_limit_active);
    TEST_ASSERT_TRUE(filtered.right_upper_limit_active);
    TEST_ASSERT_TRUE(filtered.right_lower_limit_active);

    filtered = desk_input_filter_update(&filter, NULL, 1);
    TEST_ASSERT_TRUE(filtered.emergency_active);
    TEST_ASSERT_TRUE(filtered.left_upper_limit_active);
    TEST_ASSERT_TRUE(filtered.left_lower_limit_active);
    TEST_ASSERT_TRUE(filtered.right_upper_limit_active);
    TEST_ASSERT_TRUE(filtered.right_lower_limit_active);
}

int main(void)
{
    emergency_activates_immediately();
    emergency_clear_is_debounced();
    limit_clear_is_debounced();
    buttons_pass_through_without_filter_delay();
    null_arguments_fail_safe();

    return 0;
}
