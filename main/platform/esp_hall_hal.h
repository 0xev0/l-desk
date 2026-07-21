#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_types.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

typedef struct {
    pcnt_unit_handle_t units[DESK_AXIS_COUNT];
    pcnt_channel_handle_t channels_a[DESK_AXIS_COUNT];
    pcnt_channel_handle_t channels_b[DESK_AXIS_COUNT];
    bool initialized;
} esp_hall_hal_t;

esp_err_t esp_hall_hal_init(esp_hall_hal_t *hal);
esp_err_t esp_hall_hal_read(const esp_hall_hal_t *hal, desk_inputs_t *inputs);
esp_err_t esp_hall_hal_clear(esp_hall_hal_t *hal);
