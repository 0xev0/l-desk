#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_types.h"

typedef struct {
    desk_inputs_t stable_inputs;
    uint32_t release_debounce_ms;
    uint32_t emergency_release_elapsed_ms;
    uint32_t left_upper_release_elapsed_ms;
    uint32_t left_lower_release_elapsed_ms;
    uint32_t right_upper_release_elapsed_ms;
    uint32_t right_lower_release_elapsed_ms;
} desk_input_filter_t;

void desk_input_filter_init(desk_input_filter_t *filter, uint32_t release_debounce_ms);
desk_inputs_t desk_input_filter_update(desk_input_filter_t *filter, const desk_inputs_t *raw_inputs, uint32_t elapsed_ms);
