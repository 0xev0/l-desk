#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_types.h"

typedef struct {
    bool (*initialize)(void *context);
    bool (*read_inputs)(void *context, desk_inputs_t *inputs);
    bool (*apply_outputs)(void *context, const desk_outputs_t *outputs);
    bool (*zero_hall_counts)(void *context);
    bool (*service_watchdog)(void *context);
    uint64_t (*now_ms)(void *context);
    void (*delay_ms)(void *context, uint32_t delay_ms);
    void (*log_transition)(void *context,
                           const desk_outputs_t *previous,
                           const desk_outputs_t *current);
    void *context;
} desk_platform_t;
