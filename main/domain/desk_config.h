#pragma once

#include <stdint.h>

typedef struct {
    uint32_t control_tick_ms;
    uint32_t direction_change_deadtime_ms;
    uint32_t alignment_timeout_ms;
    uint32_t calibration_timeout_ms;
    uint32_t button_debounce_ms;
} desk_config_t;

desk_config_t desk_config_default(void);
