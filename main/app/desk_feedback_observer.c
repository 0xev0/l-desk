#include "desk_feedback_observer.h"

#include <stddef.h>

static desk_feedback_diagnostics_t invalid_input_diagnostics(void)
{
    return (desk_feedback_diagnostics_t){
        .status = DESK_FEEDBACK_DIAGNOSTIC_FAULT,
        .error = DESK_FEEDBACK_ERROR_INVALID_INPUT,
        .feedback_fault = true,
    };
}

void desk_feedback_observer_init(desk_feedback_observer_t *observer,
                                 desk_feedback_source_t source,
                                 desk_feedback_config_t config)
{
    if (observer == NULL) {
        return;
    }

    observer->source = source;
    observer->config = config;
    desk_feedback_monitor_init(&observer->monitor);
}

desk_feedback_diagnostics_t desk_feedback_observer_poll(desk_feedback_observer_t *observer)
{
    if (observer == NULL || observer->source.read == NULL) {
        return invalid_input_diagnostics();
    }

    desk_feedback_inputs_t inputs = observer->source.read(observer->source.context);
    return desk_feedback_monitor_update(&observer->monitor, &observer->config, &inputs);
}
