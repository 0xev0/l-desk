#pragma once

#include "desk_platform.h"

typedef struct {
    desk_inputs_t inputs;
    desk_outputs_t outputs;
    uint64_t now_ms;
    unsigned initialize_calls;
    unsigned apply_calls;
    unsigned zero_calls;
    unsigned log_calls;
    bool fail_initialize;
    bool fail_read;
    bool fail_apply;
    bool fail_zero;
    bool fail_watchdog;
} fake_platform_t;

void fake_platform_reset(fake_platform_t *fake);
desk_platform_t fake_platform_interface(fake_platform_t *fake);
