#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t control_tick_ms;
    uint32_t max_control_interval_ms;
    uint32_t safe_start_release_ms;
    uint32_t direction_change_deadtime_ms;
    uint32_t alignment_timeout_ms;
    uint32_t calibration_timeout_ms;
    uint32_t button_debounce_ms;
    uint32_t calibrate_hold_ms;
    uint32_t soft_start_ms;
    uint32_t feedback_stall_timeout_ms;
    int32_t feedback_warning_counts;
    int32_t feedback_stop_counts;
    uint16_t maximum_duty_permille;
    uint16_t starting_duty_permille;
    uint16_t synchronization_trim_permille;
    bool feedback_required;
} desk_config_t;

desk_config_t desk_config_default(void);
