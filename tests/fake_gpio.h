#pragma once

#include <stdbool.h>

#include "desk_gpio.h"

void fake_gpio_reset(void);
void fake_gpio_set_input(desk_gpio_input_t input, bool active);
bool fake_gpio_get_output(desk_gpio_output_t output);
const desk_gpio_hal_t *fake_gpio_hal(void);
