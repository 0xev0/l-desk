#include "controller_test_helpers.h"

static void boot_requires_released_controls(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_SAFE_START, rig.outputs.state);
    assert_all_stopped(rig.outputs);

    rig.inputs.up_pressed = true;
    controller_run_for(&rig, 1000U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_SAFE_START, rig.outputs.state);
    assert_all_stopped(rig.outputs);

    rig.inputs.up_pressed = false;
    controller_reach_idle(&rig);
}

static void manual_hold_to_run_controls_three_axes(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);

    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_UP, rig.outputs.state);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_UP);

    rig.inputs.up_pressed = false;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig.outputs.state);
    assert_all_stopped(rig.outputs);

    rig.inputs.down_pressed = true;
    controller_run_for(&rig, config.direction_change_deadtime_ms + config.button_debounce_ms + 20U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_MOVING_DOWN, rig.outputs.state);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_DOWN);
}

static void soft_start_ramps_without_exceeding_maximum(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    uint16_t initial = rig.outputs.motor[0].duty_permille;
    TEST_ASSERT_TRUE(initial >= config.starting_duty_permille);

    controller_run_for(&rig, config.soft_start_ms);
    TEST_ASSERT_EQUAL_INT(config.maximum_duty_permille, rig.outputs.motor[0].duty_permille);
}

static void referenced_boot_requires_deliberate_calibration(void)
{
    controller_test_rig_t rig;
    desk_config_t config = desk_config_default();
    config.feedback_required = false;
    controller_rig_init(&rig, &config);
    controller_run_for(&rig, config.safe_start_release_ms + config.control_tick_ms);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_UNREFERENCED, rig.outputs.state);
    TEST_ASSERT_FALSE(rig.outputs.position_reference_valid);

    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_UNREFERENCED, rig.outputs.state);
    assert_all_stopped(rig.outputs);

    rig.inputs.up_pressed = false;
    rig.inputs.calibrate_pressed = true;
    controller_run_for(&rig, config.calibrate_hold_ms + config.control_tick_ms);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_CALIBRATING_DOWN, rig.outputs.state);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        rig.inputs.axis[axis].lower_limit_active = true;
    }
    (void)controller_tick_by(&rig, config.control_tick_ms);
    TEST_ASSERT_TRUE(rig.outputs.calibration_completed);
    TEST_ASSERT_TRUE(rig.outputs.position_reference_valid);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig.outputs.state);
}

int main(void)
{
    boot_requires_released_controls();
    manual_hold_to_run_controls_three_axes();
    soft_start_ramps_without_exceeding_maximum();
    referenced_boot_requires_deliberate_calibration();
    return 0;
}
