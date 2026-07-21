#include "controller_test_helpers.h"

static void release_gap_cannot_bypass_reversal_deadtime(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_UP);

    rig.inputs.up_pressed = false;
    (void)controller_tick_by(&rig, 10U);
    rig.inputs.down_pressed = true;
    controller_run_for(&rig, config.button_debounce_ms + 100U);
    assert_all_stopped(rig.outputs);

    controller_run_for(&rig, config.direction_change_deadtime_ms);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_DOWN);
}

static void long_idle_satisfies_deadtime(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    rig.inputs.up_pressed = false;
    (void)controller_tick_by(&rig, 10U);
    controller_run_for(&rig, config.direction_change_deadtime_ms);
    rig.inputs.down_pressed = true;
    controller_debounce_button(&rig);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_DOWN);
}

int main(void)
{
    release_gap_cannot_bypass_reversal_deadtime();
    long_idle_satisfies_deadtime();
    return 0;
}
