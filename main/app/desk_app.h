#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_controller.h"
#include "desk_command_arbiter.h"
#include "desk_input_filter.h"
#include "desk_platform.h"

typedef struct {
    size_t current_free_heap_bytes;
    size_t minimum_free_heap_bytes;
    uint32_t dropped_log_records;
    uint32_t control_stack_high_water_words;
    uint32_t logger_stack_high_water_words;
} desk_runtime_diagnostics_t;

typedef struct {
    desk_controller_t controller;
    desk_command_arbiter_t command_arbiter;
    desk_input_filter_t input_filter;
    desk_platform_t platform;
    desk_outputs_t last_outputs;
    uint64_t last_step_ms;
    uint64_t clear_error_pressed_since_ms;
    desk_control_source_t last_control_source;
    bool remote_control_active;
    bool clear_error_timing;
    bool clear_error_armed;
    bool initialized;
} desk_app_t;

bool desk_app_init(desk_app_t *app, const desk_platform_t *platform, const desk_config_t *config);
desk_outputs_t desk_app_step(desk_app_t *app);
bool desk_app_set_control_mode(desk_app_t *app, desk_control_mode_t mode);
bool desk_app_acquire_remote_lease(desk_app_t *app, uint64_t lease_id);
bool desk_app_submit_remote_command(desk_app_t *app, const desk_remote_command_t *command);
bool desk_app_start(void);
bool desk_app_get_runtime_diagnostics(desk_runtime_diagnostics_t *diagnostics);
bool desk_app_queue_remote_lease(uint64_t lease_id);
bool desk_app_queue_remote_command(const desk_remote_command_t *command);
bool desk_app_queue_control_mode(desk_control_mode_t mode);
