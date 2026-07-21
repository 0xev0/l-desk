#include "controller_test_helpers.h"

static void all_endstop_arrival_orders_succeed(void)
{
    const size_t orders[][DESK_AXIS_COUNT] = {
        {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0},
    };

    for (size_t order = 0; order < sizeof(orders) / sizeof(orders[0]); ++order) {
        controller_test_rig_t rig;
        desk_config_t config = controller_test_config();
        controller_rig_init(&rig, &config);
        controller_reach_idle(&rig);
        rig.inputs.up_pressed = true;
        controller_debounce_button(&rig);
        for (size_t arrival = 0; arrival < DESK_AXIS_COUNT; ++arrival) {
            rig.inputs.axis[orders[order][arrival]].upper_limit_active = true;
            (void)controller_tick_by(&rig, 10U);
        }
        TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig.outputs.state);
        assert_all_stopped(rig.outputs);
    }
}

static void missing_endstop_times_out(void)
{
    controller_test_rig_t rig;
    desk_config_t config = controller_test_config();
    controller_rig_init(&rig, &config);
    controller_reach_idle(&rig);
    rig.inputs.up_pressed = true;
    controller_debounce_button(&rig);
    rig.inputs.axis[0].upper_limit_active = true;
    (void)controller_tick_by(&rig, 10U);
    controller_run_for(&rig, config.alignment_timeout_ms);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_ERROR, rig.outputs.state);
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_ALIGNMENT_TIMEOUT, rig.outputs.error);
    assert_all_stopped(rig.outputs);
}

int main(void)
{
    all_endstop_arrival_orders_succeed();
    missing_endstop_times_out();
    return 0;
}
