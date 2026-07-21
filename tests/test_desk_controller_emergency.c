#include "controller_test_helpers.h"

static void emergency_latches_error_from_every_motion_state(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    rig.inputs.emergency_active = true;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, rig.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_EMERGENCY_STOP, rig.outputs.error);
    assert_all_stopped(rig.outputs);

    rig.inputs.emergency_active = false;
    rig.inputs.up_pressed = false;
    TEST_ASSERT_TRUE(desk_controller_request_clear_error(&rig.controller, &rig.inputs, rig.now_ms));
    rig.outputs = desk_controller_tick(&rig.controller, &rig.inputs, rig.now_ms + 10U);
    rig.now_ms += 10U;
    TEST_ASSERT_EQUAL_INT(DESK_STATE_SAFE_START, rig.outputs.state);
    assert_all_stopped(rig.outputs);
}

static void unsafe_inputs_cannot_clear_error(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    rig.inputs.emergency_active = true;
    (void)controller_tick_by(&rig, 10U);
    TEST_ASSERT_FALSE(
        desk_controller_request_clear_error(&rig.controller, &rig.inputs, rig.now_ms));

    rig.inputs.emergency_active = false;
    rig.inputs.hardware_fault = true;
    TEST_ASSERT_FALSE(
        desk_controller_request_clear_error(&rig.controller, &rig.inputs, rig.now_ms));
}

int main(void)
{
    emergency_latches_error_from_every_motion_state();
    unsafe_inputs_cannot_clear_error();
    return 0;
}
