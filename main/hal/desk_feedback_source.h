#pragma once

#include "desk_feedback.h"

typedef struct {
    desk_feedback_inputs_t (*read)(void *context);
    void *context;
} desk_feedback_source_t;
