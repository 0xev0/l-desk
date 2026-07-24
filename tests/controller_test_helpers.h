#pragma once

#include "desk_controller.h"
#include "test_support.h"

typedef struct {
    desk_controller_t controller;
    desk_inputs_t inputs;
    desk_outputs_t outputs;
    uint64_t now_ms;
} controller_test_rig_t;

static inline desk_config_t controller_test_config(void)
{
    desk_config_t config = desk_config_default();
    config.feedback_warning_um = 3000;
    config.feedback_stop_um = 5000;
    config.require_position_reference = false;
    return config;
}

static inline desk_inputs_t controller_healthy_inputs(void)
{
    desk_inputs_t inputs = {0};
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        inputs.axis[axis].hall_valid = true;
    }
    return inputs;
}

static inline desk_outputs_t controller_tick_by(controller_test_rig_t *rig, uint32_t elapsed_ms)
{
    rig->now_ms += elapsed_ms;
    rig->outputs = desk_controller_tick(&rig->controller, &rig->inputs, rig->now_ms);
    return rig->outputs;
}

static inline void controller_run_for(controller_test_rig_t *rig, uint32_t duration_ms)
{
    uint32_t ticks = duration_ms / rig->controller.config.control_tick_ms;
    for (uint32_t tick = 0; tick < ticks; ++tick) {
        (void)controller_tick_by(rig, rig->controller.config.control_tick_ms);
    }
}

static inline void controller_rig_init(controller_test_rig_t *rig, const desk_config_t *config)
{
    *rig = (controller_test_rig_t){
        .inputs = controller_healthy_inputs(),
    };
    desk_controller_init(&rig->controller, config);
    rig->outputs = desk_controller_tick(&rig->controller, &rig->inputs, 0U);
}

static inline void controller_reach_idle(controller_test_rig_t *rig)
{
    controller_run_for(rig, rig->controller.config.safe_start_release_ms +
                                rig->controller.config.control_tick_ms);
    TEST_ASSERT_EQUAL_INT(DESK_STATE_IDLE, rig->outputs.state);
}

static inline void controller_debounce_button(controller_test_rig_t *rig)
{
    controller_run_for(rig, rig->controller.config.button_debounce_ms +
                                rig->controller.config.control_tick_ms);
}

static inline void assert_all_stopped(desk_outputs_t outputs)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_EQUAL_INT(MOTOR_COMMAND_STOP, outputs.motor[axis].direction);
        TEST_ASSERT_EQUAL_INT(0, outputs.motor[axis].duty_permille);
        TEST_ASSERT_FALSE(outputs.motor[axis].enable);
    }
}

static inline void assert_all_moving(desk_outputs_t outputs, motor_command_t direction)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        TEST_ASSERT_EQUAL_INT(direction, outputs.motor[axis].direction);
        TEST_ASSERT_TRUE(outputs.motor[axis].duty_permille > 0U);
        TEST_ASSERT_TRUE(outputs.motor[axis].enable);
    }
}
