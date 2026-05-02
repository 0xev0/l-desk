#include "desk_feedback.h"

#include <stddef.h>
#include <stdint.h>

static desk_feedback_diagnostics_t diagnostics_ok(void)
{
    return (desk_feedback_diagnostics_t){
        .status = DESK_FEEDBACK_DIAGNOSTIC_OK,
        .error = DESK_FEEDBACK_ERROR_NONE,
        .feedback_ok = true,
    };
}

static desk_feedback_diagnostics_t diagnostics_missing(void)
{
    return (desk_feedback_diagnostics_t){
        .status = DESK_FEEDBACK_DIAGNOSTIC_MISSING,
        .error = DESK_FEEDBACK_ERROR_MISSING,
        .feedback_missing = true,
    };
}

static desk_feedback_diagnostics_t diagnostics_fault(desk_feedback_error_t error)
{
    return (desk_feedback_diagnostics_t){
        .status = DESK_FEEDBACK_DIAGNOSTIC_FAULT,
        .error = error,
        .feedback_fault = true,
    };
}

static bool axis_status_is_known(desk_axis_feedback_status_t status)
{
    switch (status) {
    case DESK_AXIS_FEEDBACK_MISSING:
    case DESK_AXIS_FEEDBACK_VALID:
    case DESK_AXIS_FEEDBACK_FAULT:
        return true;
    default:
        return false;
    }
}

static bool axis_has_fault(desk_axis_feedback_status_t status)
{
    return status == DESK_AXIS_FEEDBACK_FAULT || !axis_status_is_known(status);
}

static bool axis_is_missing(desk_axis_feedback_status_t status)
{
    return status == DESK_AXIS_FEEDBACK_MISSING;
}

static uint32_t axis_delta_units(int32_t left_position, int32_t right_position)
{
    int64_t left = left_position;
    int64_t right = right_position;
    int64_t delta = left - right;

    if (delta < 0) {
        delta = -delta;
    }

    if (delta > INT32_MAX) {
        return (uint32_t)INT32_MAX;
    }

    return (uint32_t)delta;
}

desk_feedback_diagnostics_t desk_feedback_evaluate(const desk_feedback_config_t *config,
                                                   const desk_feedback_inputs_t *inputs)
{
    if (config == NULL ||
        inputs == NULL ||
        config->max_axis_delta_units < 0 ||
        config->max_position_step_units < 0) {
        return diagnostics_fault(DESK_FEEDBACK_ERROR_INVALID_INPUT);
    }

    if (axis_has_fault(inputs->left.status) || axis_has_fault(inputs->right.status)) {
        return diagnostics_fault(DESK_FEEDBACK_ERROR_AXIS_FAULT);
    }

    if (axis_is_missing(inputs->left.status) || axis_is_missing(inputs->right.status)) {
        return diagnostics_missing();
    }

    uint32_t delta = axis_delta_units(inputs->left.position_units, inputs->right.position_units);
    if (delta > (uint32_t)config->max_axis_delta_units) {
        return diagnostics_fault(DESK_FEEDBACK_ERROR_AXIS_DISAGREEMENT);
    }

    return diagnostics_ok();
}

void desk_feedback_monitor_init(desk_feedback_monitor_t *monitor)
{
    if (monitor == NULL) {
        return;
    }

    *monitor = (desk_feedback_monitor_t){0};
}

desk_feedback_diagnostics_t desk_feedback_monitor_update(desk_feedback_monitor_t *monitor,
                                                         const desk_feedback_config_t *config,
                                                         const desk_feedback_inputs_t *inputs)
{
    if (monitor == NULL) {
        return diagnostics_fault(DESK_FEEDBACK_ERROR_INVALID_INPUT);
    }

    desk_feedback_diagnostics_t diagnostics = desk_feedback_evaluate(config, inputs);
    if (diagnostics.status != DESK_FEEDBACK_DIAGNOSTIC_OK) {
        monitor->has_previous_valid_sample = false;
        return diagnostics;
    }

    if (monitor->has_previous_valid_sample) {
        uint32_t left_step = axis_delta_units(monitor->previous_inputs.left.position_units,
                                             inputs->left.position_units);
        uint32_t right_step = axis_delta_units(monitor->previous_inputs.right.position_units,
                                              inputs->right.position_units);

        if (left_step > (uint32_t)config->max_position_step_units ||
            right_step > (uint32_t)config->max_position_step_units) {
            monitor->has_previous_valid_sample = false;
            return diagnostics_fault(DESK_FEEDBACK_ERROR_POSITION_JUMP);
        }
    }

    monitor->previous_inputs = *inputs;
    monitor->has_previous_valid_sample = true;
    return diagnostics;
}
