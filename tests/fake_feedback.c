#include "fake_feedback.h"

#include <stddef.h>

static desk_feedback_inputs_t missing_inputs(void)
{
    return (desk_feedback_inputs_t){
        .left = {
            .status = DESK_AXIS_FEEDBACK_MISSING,
        },
        .right = {
            .status = DESK_AXIS_FEEDBACK_MISSING,
        },
    };
}

static desk_feedback_inputs_t read_feedback(void *context)
{
    if (context == NULL) {
        return missing_inputs();
    }

    fake_feedback_t *fake = (fake_feedback_t *)context;
    return fake->inputs;
}

void fake_feedback_reset(fake_feedback_t *fake)
{
    if (fake == NULL) {
        return;
    }

    fake->inputs = missing_inputs();
}

void fake_feedback_set_inputs(fake_feedback_t *fake, desk_feedback_inputs_t inputs)
{
    if (fake == NULL) {
        return;
    }

    fake->inputs = inputs;
}

desk_feedback_source_t fake_feedback_source(fake_feedback_t *fake)
{
    return (desk_feedback_source_t){
        .read = read_feedback,
        .context = fake,
    };
}
