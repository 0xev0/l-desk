#include "controller_test_helpers.h"
#include "motor_axis.h"

#define INPUT_COMBINATION_COUNT (1U << 11U)

static desk_inputs_t inputs_from_mask(uint32_t mask)
{
    desk_inputs_t inputs = controller_healthy_inputs();
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        inputs.axis[axis].upper_limit_active = (mask & (1U << (axis * 2U))) != 0U;
        inputs.axis[axis].lower_limit_active = (mask & (1U << (axis * 2U + 1U))) != 0U;
    }
    inputs.up_pressed = (mask & (1U << 6U)) != 0U;
    inputs.down_pressed = (mask & (1U << 7U)) != 0U;
    inputs.calibrate_pressed = (mask & (1U << 8U)) != 0U;
    inputs.emergency_active = (mask & (1U << 9U)) != 0U;
    inputs.hardware_fault = (mask & (1U << 10U)) != 0U;
    return inputs;
}

static void assert_output_invariants(desk_outputs_t outputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_TRUE(motor_axis_command_is_valid(&outputs.motor[axis]));
        motor_axis_output_t physical = motor_axis_command_to_output(&outputs.motor[axis]);
        TEST_ASSERT_TRUE(motor_axis_output_is_safe(&physical));
        TEST_ASSERT_FALSE(physical.rpwm_active && physical.lpwm_active);
    }
    if (outputs.state == DESK_STATE_ERROR) {
        assert_all_stopped(outputs);
    }
}

static void every_boolean_input_combination_is_fail_safe(void)
{
    desk_config_t config = controller_test_config();
    config.feedback_required = false;
    config.safe_start_release_ms = 0U;
    config.button_debounce_ms = 0U;
    config.calibrate_hold_ms = 0U;

    for (uint32_t mask = 0U; mask < INPUT_COMBINATION_COUNT; ++mask) {
        controller_test_rig_t rig;
        controller_rig_init(&rig, &config);
        (void)controller_tick_by(&rig, config.control_tick_ms);
        rig.inputs = inputs_from_mask(mask);
        desk_outputs_t outputs = controller_tick_by(&rig, config.control_tick_ms);
        assert_output_invariants(outputs);
    }
}

int main(void)
{
    every_boolean_input_combination_is_fail_safe();
    return 0;
}
