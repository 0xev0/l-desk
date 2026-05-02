#pragma once

#include "desk_feedback.h"
#include "desk_feedback_source.h"

typedef struct {
    desk_feedback_source_t source;
    desk_feedback_config_t config;
    desk_feedback_monitor_t monitor;
} desk_feedback_observer_t;

void desk_feedback_observer_init(desk_feedback_observer_t *observer,
                                 desk_feedback_source_t source,
                                 desk_feedback_config_t config);
desk_feedback_diagnostics_t desk_feedback_observer_poll(desk_feedback_observer_t *observer);
