#include "fake_platform.h"

#include <stddef.h>

static bool initialize(void *context)
{
    fake_platform_t *fake = context;
    if (fake == NULL) {
        return false;
    }
    ++fake->initialize_calls;
    return !fake->fail_initialize;
}

static bool read_inputs(void *context, desk_inputs_t *inputs)
{
    fake_platform_t *fake = context;
    if (fake == NULL || inputs == NULL || fake->fail_read) {
        return false;
    }
    *inputs = fake->inputs;
    return true;
}

static bool apply_outputs(void *context, const desk_outputs_t *outputs)
{
    fake_platform_t *fake = context;
    if (fake == NULL || outputs == NULL) {
        return false;
    }
    ++fake->apply_calls;
    fake->outputs = *outputs;
    return !fake->fail_apply;
}

static bool zero_hall_counts(void *context)
{
    fake_platform_t *fake = context;
    if (fake == NULL) {
        return false;
    }
    ++fake->zero_calls;
    return !fake->fail_zero;
}

static uint64_t now_ms(void *context)
{
    fake_platform_t *fake = context;
    return fake != NULL ? fake->now_ms : 0U;
}

static bool service_watchdog(void *context)
{
    fake_platform_t *fake = context;
    return fake != NULL && !fake->fail_watchdog;
}

static void log_transition(void *context, const desk_outputs_t *previous,
                           const desk_outputs_t *current)
{
    fake_platform_t *fake = context;
    if (fake != NULL && previous != NULL && current != NULL) {
        ++fake->log_calls;
    }
}

void fake_platform_reset(fake_platform_t *fake)
{
    if (fake == NULL) {
        return;
    }
    *fake = (fake_platform_t){0};
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        fake->inputs.axis[axis].hall_valid = true;
    }
}

desk_platform_t fake_platform_interface(fake_platform_t *fake)
{
    return (desk_platform_t){
        .initialize = initialize,
        .read_inputs = read_inputs,
        .apply_outputs = apply_outputs,
        .zero_hall_counts = zero_hall_counts,
        .service_watchdog = service_watchdog,
        .now_ms = now_ms,
        .log_transition = log_transition,
        .context = fake,
    };
}
