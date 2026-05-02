#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DESK_AXIS_FEEDBACK_MISSING = 0,
    DESK_AXIS_FEEDBACK_VALID,
    DESK_AXIS_FEEDBACK_FAULT,
} desk_axis_feedback_status_t;

typedef enum {
    DESK_FEEDBACK_DIAGNOSTIC_OK = 0,
    DESK_FEEDBACK_DIAGNOSTIC_MISSING,
    DESK_FEEDBACK_DIAGNOSTIC_FAULT,
} desk_feedback_diagnostic_status_t;

typedef enum {
    DESK_FEEDBACK_ERROR_NONE = 0,
    DESK_FEEDBACK_ERROR_MISSING,
    DESK_FEEDBACK_ERROR_AXIS_FAULT,
    DESK_FEEDBACK_ERROR_AXIS_DISAGREEMENT,
    DESK_FEEDBACK_ERROR_POSITION_JUMP,
    DESK_FEEDBACK_ERROR_INVALID_INPUT,
} desk_feedback_error_t;

typedef struct {
    desk_axis_feedback_status_t status;
    int32_t position_units;
} desk_axis_feedback_t;

typedef struct {
    desk_axis_feedback_t left;
    desk_axis_feedback_t right;
} desk_feedback_inputs_t;

typedef struct {
    int32_t max_axis_delta_units;
    int32_t max_position_step_units;
} desk_feedback_config_t;

typedef struct {
    desk_feedback_diagnostic_status_t status;
    desk_feedback_error_t error;
    bool feedback_ok;
    bool feedback_missing;
    bool feedback_fault;
} desk_feedback_diagnostics_t;

typedef struct {
    desk_feedback_inputs_t previous_inputs;
    bool has_previous_valid_sample;
} desk_feedback_monitor_t;

desk_feedback_diagnostics_t desk_feedback_evaluate(const desk_feedback_config_t *config,
                                                   const desk_feedback_inputs_t *inputs);
void desk_feedback_monitor_init(desk_feedback_monitor_t *monitor);
desk_feedback_diagnostics_t desk_feedback_monitor_update(desk_feedback_monitor_t *monitor,
                                                         const desk_feedback_config_t *config,
                                                         const desk_feedback_inputs_t *inputs);
