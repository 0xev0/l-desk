#pragma once

#include "desk_types.h"

typedef struct {
    int rpwm_pin;
    int lpwm_pin;
    int enable_pin;
} bts7960_driver_t;

void bts7960_driver_init(const bts7960_driver_t *driver);
void bts7960_driver_apply(const bts7960_driver_t *driver, motor_command_t command);
void bts7960_driver_stop(const bts7960_driver_t *driver);
