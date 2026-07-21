#pragma once

#include <stdbool.h>

#include "bts7960_driver.h"
#include "desk_types.h"
#include "esp_err.h"

typedef struct {
    bts7960_driver_t drivers[DESK_AXIS_COUNT];
    bool initialized;
} esp_gpio_hal_t;

esp_err_t esp_gpio_hal_init(esp_gpio_hal_t *hal);
esp_err_t esp_gpio_hal_read_controls(const esp_gpio_hal_t *hal, desk_inputs_t *inputs);
esp_err_t esp_gpio_hal_apply_outputs(esp_gpio_hal_t *hal, const desk_outputs_t *outputs);
esp_err_t esp_gpio_hal_stop_all(esp_gpio_hal_t *hal);
