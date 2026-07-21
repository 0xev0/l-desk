#pragma once

#include "desk_platform.h"
#include "esp_gpio_hal.h"
#include "esp_hall_hal.h"

typedef struct {
    esp_gpio_hal_t gpio;
    esp_hall_hal_t hall;
    bool watchdog_registered;
} esp_desk_platform_context_t;

desk_platform_t esp_desk_platform_create(esp_desk_platform_context_t *context);
