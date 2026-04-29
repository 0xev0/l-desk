#include "fake_time.h"

#include <limits.h>

static uint32_t now_ms;

void fake_time_reset(void)
{
    now_ms = 0;
}

void fake_time_advance_ms(uint32_t elapsed_ms)
{
    if (UINT_MAX - now_ms < elapsed_ms) {
        now_ms = UINT_MAX;
        return;
    }

    now_ms += elapsed_ms;
}

uint32_t fake_time_now_ms(void)
{
    return now_ms;
}
