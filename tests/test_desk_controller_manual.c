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

int main(void)
{
    boot_requires_released_controls();
    manual_hold_to_run_controls_three_axes();
    soft_start_ramps_without_exceeding_maximum();
    return 0;
}
