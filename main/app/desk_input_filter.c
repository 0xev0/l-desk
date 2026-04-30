#include "desk_input_filter.h"

#include <stddef.h>

static desk_inputs_t fail_safe_inputs(void)
{
    return (desk_inputs_t){
        .emergency_active = true,
        .left_upper_limit_active = true,
        .left_lower_limit_active = true,
        .right_upper_limit_active = true,
        .right_lower_limit_active = true,
    };
}

static void update_immediate_active_debounced_release(bool raw_active,
                                                     bool *stable_active,
                                                     uint32_t *release_elapsed_ms,
                                                     uint32_t release_debounce_ms,
                                                     uint32_t elapsed_ms)
{
    if (raw_active) {
        *stable_active = true;
        *release_elapsed_ms = 0;
        return;
    }

    if (!*stable_active) {
        *release_elapsed_ms = 0;
        return;
    }

    if (release_debounce_ms == 0 ||
        elapsed_ms >= release_debounce_ms ||
        *release_elapsed_ms >= release_debounce_ms - elapsed_ms) {
        *stable_active = false;
        *release_elapsed_ms = 0;
        return;
    }

    *release_elapsed_ms += elapsed_ms;
}

void desk_input_filter_init(desk_input_filter_t *filter, uint32_t release_debounce_ms)
{
    if (filter == NULL) {
        return;
    }

    *filter = (desk_input_filter_t){
        .release_debounce_ms = release_debounce_ms,
    };
}

desk_inputs_t desk_input_filter_update(desk_input_filter_t *filter, const desk_inputs_t *raw_inputs, uint32_t elapsed_ms)
{
    if (filter == NULL || raw_inputs == NULL) {
        return fail_safe_inputs();
    }

    filter->stable_inputs.up_pressed = raw_inputs->up_pressed;
    filter->stable_inputs.down_pressed = raw_inputs->down_pressed;
    filter->stable_inputs.calibrate_pressed = raw_inputs->calibrate_pressed;

    update_immediate_active_debounced_release(raw_inputs->emergency_active,
                                             &filter->stable_inputs.emergency_active,
                                             &filter->emergency_release_elapsed_ms,
                                             filter->release_debounce_ms,
                                             elapsed_ms);
    update_immediate_active_debounced_release(raw_inputs->left_upper_limit_active,
                                             &filter->stable_inputs.left_upper_limit_active,
                                             &filter->left_upper_release_elapsed_ms,
                                             filter->release_debounce_ms,
                                             elapsed_ms);
    update_immediate_active_debounced_release(raw_inputs->left_lower_limit_active,
                                             &filter->stable_inputs.left_lower_limit_active,
                                             &filter->left_lower_release_elapsed_ms,
                                             filter->release_debounce_ms,
                                             elapsed_ms);
    update_immediate_active_debounced_release(raw_inputs->right_upper_limit_active,
                                             &filter->stable_inputs.right_upper_limit_active,
                                             &filter->right_upper_release_elapsed_ms,
                                             filter->release_debounce_ms,
                                             elapsed_ms);
    update_immediate_active_debounced_release(raw_inputs->right_lower_limit_active,
                                             &filter->stable_inputs.right_lower_limit_active,
                                             &filter->right_lower_release_elapsed_ms,
                                             filter->release_debounce_ms,
                                             elapsed_ms);

    return filter->stable_inputs;
}
