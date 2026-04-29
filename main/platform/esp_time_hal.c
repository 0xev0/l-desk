#include "esp_time_hal.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint32_t desk_time_get_ms(void)
{
    int64_t now_us = esp_timer_get_time();
    if (now_us <= 0) {
        return 0;
    }

    return (uint32_t)(now_us / 1000);
}

void desk_time_delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}
