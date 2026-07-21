#include "controller_test_helpers.h"

static void each_matching_limit_stops_only_its_axis(void)
{
    for (size_t selected = 0; selected < DESK_AXIS_COUNT; ++selected) {
        controller_test_rig_t rig;
        desk_config_t config = controller_test_config();
        controller_rig_init(&rig, &config);
        controller_reach_idle(&rig);
        rig.inputs.axis[selected].upper_limit_active = true;
        rig.inputs.up_pressed = true;
        controller_debounce_button(&rig);

        for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
            TEST_ASSERT_EQUAL_INT(axis == selected ? MOTOR_COMMAND_STOP : MOTOR_COMMAND_UP,
                                  rig.outputs.motor[axis].direction);
        }
    }
}

static void opposite_limits_allow_motion_away(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    rig.inputs.axis[DESK_AXIS_WING].upper_limit_active = true;
    controller_reach_idle(&rig);
    rig.inputs.down_pressed = true;
    controller_debounce_button(&rig);
    assert_all_moving(rig.outputs, MOTOR_COMMAND_DOWN);
}

static void conflicting_limits_on_any_axis_are_faults(void)
{
    for (size_t selected = 0; selected < DESK_AXIS_COUNT; ++selected) {
        controller_test_rig_t rig;
        desk_config_t config = controller_test_config();
        controller_rig_init(&rig, &config);
        rig.inputs.axis[selected].upper_limit_active = true;
        rig.inputs.axis[selected].lower_limit_active = true;
        (void)controller_tick_by(&rig, 10U);
        TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, rig.outputs.state);
        TEST_ASSERT_EQUAL_INT(DESK_ERROR_LIMIT_CONFLICT, rig.outputs.error);
        assert_all_stopped(rig.outputs);
    }
}

int main(void)
{
    each_matching_limit_stops_only_its_axis();
    opposite_limits_allow_motion_away();
    conflicting_limits_on_any_axis_are_faults();
    return 0;
}
