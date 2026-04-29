#pragma once

#include <stdbool.h>

#include "desk_types.h"

typedef enum {
    DESK_GPIO_INPUT_BUTTON_UP = 0,
    DESK_GPIO_INPUT_BUTTON_DOWN,
    DESK_GPIO_INPUT_BUTTON_CALIBRATE,
    DESK_GPIO_INPUT_EMERGENCY_STOP,
    DESK_GPIO_INPUT_LEFT_LIMIT_UP,
    DESK_GPIO_INPUT_LEFT_LIMIT_DOWN,
    DESK_GPIO_INPUT_RIGHT_LIMIT_UP,
    DESK_GPIO_INPUT_RIGHT_LIMIT_DOWN,
} desk_gpio_input_t;

typedef enum {
    DESK_GPIO_OUTPUT_LEFT_RPWM = 0,
    DESK_GPIO_OUTPUT_LEFT_LPWM,
    DESK_GPIO_OUTPUT_LEFT_ENABLE,
    DESK_GPIO_OUTPUT_RIGHT_RPWM,
    DESK_GPIO_OUTPUT_RIGHT_LPWM,
    DESK_GPIO_OUTPUT_RIGHT_ENABLE,
} desk_gpio_output_t;

typedef struct {
    bool (*read_input)(desk_gpio_input_t input);
    void (*set_output)(desk_gpio_output_t output, bool active);
} desk_gpio_hal_t;
