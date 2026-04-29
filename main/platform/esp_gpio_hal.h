#pragma once

#include "desk_types.h"

void esp_gpio_hal_init(void);
desk_inputs_t esp_gpio_hal_read_inputs(void);
void esp_gpio_hal_apply_outputs(const desk_outputs_t *outputs);
