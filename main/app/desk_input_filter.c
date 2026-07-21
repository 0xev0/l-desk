#include "desk_input_filter.h"

#include <stddef.h>

static desk_inputs_t fail_safe_inputs(void)
{
    desk_inputs_t inputs = {
        .emergency_active = true,
        .hardware_fault = true,
    };
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        inputs.axis[axis].upper_limit_active = true;
        inputs.axis[axis].lower_limit_active = true;
        inputs.axis[axis].hall_valid = false;
    }
    return inputs;
}

static void update_immediate_active_debounced_release(bool raw_active, bool *stable_active,
                                                      uint32_t *release_elapsed_ms,
                                                      uint32_t release_debounce_ms,
                                                      uint32_t elapsed_ms)
{
    if (raw_active) {
        *stable_active = true;
        *release_elapsed_ms = 0U;
        return;
    }
    if (!*stable_active) {
        *release_elapsed_ms = 0U;
        return;
    }
    if (release_debounce_ms == 0U || elapsed_ms >= release_debounce_ms ||
        *release_elapsed_ms >= release_debounce_ms - elapsed_ms) {
        *stable_active = false;
        *release_elapsed_ms = 0U;
        return;
    }
    *release_elapsed_ms += elapsed_ms;
}

void desk_input_filter_init(desk_input_filter_t *filter, uint32_t release_debounce_ms)
{
    if (filter != NULL) {
        *filter = (desk_input_filter_t){
            .release_debounce_ms = release_debounce_ms,
        };
    }
}

desk_inputs_t desk_input_filter_update(desk_input_filter_t *filter, const desk_inputs_t *raw_inputs,
                                       uint32_t elapsed_ms)
{
    if (filter == NULL || raw_inputs == NULL) {
        return fail_safe_inputs();
    }

    filter->stable_inputs.up_pressed = raw_inputs->up_pressed;
    filter->stable_inputs.down_pressed = raw_inputs->down_pressed;
    filter->stable_inputs.calibrate_pressed = raw_inputs->calibrate_pressed;
    filter->stable_inputs.hardware_fault = raw_inputs->hardware_fault;
    update_immediate_active_debounced_release(
        raw_inputs->emergency_active, &filter->stable_inputs.emergency_active,
        &filter->emergency_release_elapsed_ms, filter->release_debounce_ms, elapsed_ms);

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        filter->stable_inputs.axis[axis].hall_count = raw_inputs->axis[axis].hall_count;
        filter->stable_inputs.axis[axis].hall_valid = raw_inputs->axis[axis].hall_valid;
        update_immediate_active_debounced_release(
            raw_inputs->axis[axis].upper_limit_active,
            &filter->stable_inputs.axis[axis].upper_limit_active,
            &filter->upper_release_elapsed_ms[axis], filter->release_debounce_ms, elapsed_ms);
        update_immediate_active_debounced_release(
            raw_inputs->axis[axis].lower_limit_active,
            &filter->stable_inputs.axis[axis].lower_limit_active,
            &filter->lower_release_elapsed_ms[axis], filter->release_debounce_ms, elapsed_ms);
    }
    return filter->stable_inputs;
}
