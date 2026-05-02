#pragma once

#include "desk_feedback_source.h"

typedef struct {
    desk_feedback_inputs_t inputs;
} fake_feedback_t;

void fake_feedback_reset(fake_feedback_t *fake);
void fake_feedback_set_inputs(fake_feedback_t *fake, desk_feedback_inputs_t inputs);
desk_feedback_source_t fake_feedback_source(fake_feedback_t *fake);
