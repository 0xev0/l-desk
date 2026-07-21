#include "controller_test_helpers.h"

static void calibration_requires_hold_and_release_aborts(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    config.feedback_required = false;
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.calibrate_pressed = true;
    controller_run_for(&rig, config.calibrate_hold_ms - 10U);
    assert_all_stopped(rig.outputs);
    controller_run_for(&rig, 20U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_CALIBRATING_DOWN, rig.outputs.state);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_DOWN);

    rig.inputs.calibrate_pressed = false;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig.outputs.state);
    assert_all_stopped(rig.outputs);
}

static void calibration_stops_each_axis_and_succeeds(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    config.feedback_required = false;
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.calibrate_pressed = true;
    controller_run_for(&rig, config.calibrate_hold_ms + 10U);

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        rig.inputs.axis[axis].lower_limit_active = true;
        (void)controller_tick_by(&rig, 10U);
        TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, rig.outputs.motor[axis].direction);
    }
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig.outputs.state);
    assert_all_stopped(rig.outputs);
}

static void calibration_timeout_is_distinct(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    config.feedback_required = false;
    config.calibration_timeout_ms = 100U;
    config.alignment_timeout_ms = 1000U;
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.calibrate_pressed = true;
    controller_run_for(&rig, config.calibrate_hold_ms + config.calibration_timeout_ms + 20U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, rig.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_CALIBRATION_TIMEOUT, rig.outputs.error);
}

int main(void)
{
    calibration_requires_hold_and_release_aborts();
    calibration_stops_each_axis_and_succeeds();
    calibration_timeout_is_distinct();
    return 0;
}
