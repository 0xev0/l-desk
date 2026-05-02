#include "feedback_none_source.h"

#include <stddef.h>

static desk_feedback_inputs_t read_missing_feedback(void *context)
{
    (void)context;

    return (desk_feedback_inputs_t){
        .left = {
            .status = DESK_AXIS_FEEDBACK_MISSING,
        },
        .right = {
            .status = DESK_AXIS_FEEDBACK_MISSING,
        },
    };
}

desk_feedback_source_t feedback_none_source(void)
{
    return (desk_feedback_source_t){
        .read = read_missing_feedback,
        .context = NULL,
    };
}
