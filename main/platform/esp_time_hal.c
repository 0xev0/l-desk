#include "esp_time_hal.h"

#include "esp_timer.h"

uint64_t esp_time_hal_now_ms(void)
{
    int64_t now_us = esp_timer_get_time();
    return now_us > 0 ? (uint64_t)now_us / 1000U : 0U;
}
