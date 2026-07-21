#include "controller_test_helpers.h"

static void invalid_arguments_fail_stopped(void)
{
    desk_outputs_t outputs = desk_controller_tick(NULL, NULL, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);
    assert_all_stopped(outputs);

    desk_controller_t controller;
    desk_config_t invalid_config = desk_config_default();
    invalid_config.maximum_duty_permille = 1001U;
    desk_controller_init(&controller, &invalid_config);
    outputs = desk_controller_tick(&controller, &(desk_inputs_t){0}, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_HARDWARE_FAULT, outputs.error);
    assert_all_stopped(outputs);
}

static void control_loop_overrun_is_latched(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    (void)controller_tick_by(&rig, config.max_control_interval_ms + 1U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, rig.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_CONTROL_LOOP_OVERRUN, rig.outputs.error);
}

static void both_buttons_are_latched_error(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    rig.inputs.down_pressed = true;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_BOTH_BUTTONS_PRESSED, rig.outputs.error);
    assert_all_stopped(rig.outputs);
}

static void feedback_faults_stop_all_axes(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    rig.inputs.axis[DESK_AXIS_WING].hall_valid = false;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_FEEDBACK_MISSING, rig.outputs.error);
    assert_all_stopped(rig.outputs);
}

static void feedback_direction_and_skew_are_checked(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    rig.inputs.axis[0].hall_count = -1;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_FEEDBACK_DIRECTION, rig.outputs.error);

    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.axis[0].hall_count = config.feedback_stop_counts + 1;
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_FEEDBACK_SKEW, rig.outputs.error);
}

static void feedback_stall_times_out(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    config.feedback_stall_timeout_ms = 100U;
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    controller_run_for(&rig, config.feedback_stall_timeout_ms);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_FEEDBACK_STALL, rig.outputs.error);
}

static void feedback_warning_trims_only_the_leading_axis(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    config.soft_start_ms = 0U;
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.axis[DESK_AXIS_FRONT_LEFT].hall_count = config.feedback_warning_counts;
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);

    TEST_ASSERT_TRUE(rig.outputs.feedback_warning);
    TEST_ASSERT_TRUE(rig.outputs.motor[DESK_AXIS_FRONT_LEFT].duty_permille <
                     rig.outputs.motor[DESK_AXIS_FRONT_RIGHT].duty_permille);
    TEST_ASSERT_EQUAL_INT(rig.outputs.motor[DESK_AXIS_FRONT_RIGHT].duty_permille,
                          rig.outputs.motor[DESK_AXIS_WING].duty_permille);
}

int main(void)
{
    invalid_arguments_fail_stopped();
    control_loop_overrun_is_latched();
    both_buttons_are_latched_error();
    feedback_faults_stop_all_axes();
    feedback_direction_and_skew_are_checked();
    feedback_stall_times_out();
    feedback_warning_trims_only_the_leading_axis();
    return 0;
}
