#pragma once

#include <stdint.h>

void fake_time_reset(void);
void fake_time_advance_ms(uint32_t elapsed_ms);
uint32_t fake_time_now_ms(void);
