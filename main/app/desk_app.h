#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_controller.h"
#include "desk_input_filter.h"
#include "desk_platform.h"

typedef struct {
    desk_controller_t controller;
    desk_input_filter_t input_filter;
    desk_platform_t platform;
    desk_outputs_t last_outputs;
    uint64_t last_step_ms;
    uint64_t clear_error_pressed_since_ms;
    bool clear_error_timing;
    bool clear_error_armed;
    bool initialized;
} desk_app_t;

bool desk_app_init(desk_app_t *app, const desk_platform_t *platform, const desk_config_t *config);
desk_outputs_t desk_app_step(desk_app_t *app);
void desk_app_run(void);
