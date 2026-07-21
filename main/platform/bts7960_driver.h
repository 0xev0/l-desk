#pragma once

#include <stdbool.h>

#include "driver/ledc.h"
#include "esp_err.h"

#include "desk_types.h"

typedef struct {
    int rpwm_pin;
    int lpwm_pin;
    int enable_pin;
    ledc_channel_t rpwm_channel;
    ledc_channel_t lpwm_channel;
    motor_command_t applied_direction;
    bool initialized;
} bts7960_driver_t;

esp_err_t bts7960_driver_init(bts7960_driver_t *driver);
esp_err_t bts7960_driver_apply(bts7960_driver_t *driver,
                               const desk_axis_output_t *command);
esp_err_t bts7960_driver_stop(bts7960_driver_t *driver);
