#include "fake_gpio.h"

static bool fake_inputs[DESK_GPIO_INPUT_RIGHT_LIMIT_DOWN + 1];
static bool fake_outputs[DESK_GPIO_OUTPUT_RIGHT_ENABLE + 1];

static bool read_input(desk_gpio_input_t input)
{
    if (input < DESK_GPIO_INPUT_BUTTON_UP || input > DESK_GPIO_INPUT_RIGHT_LIMIT_DOWN) {
        return false;
    }

    return fake_inputs[input];
}

static void set_output(desk_gpio_output_t output, bool active)
{
    if (output < DESK_GPIO_OUTPUT_LEFT_RPWM || output > DESK_GPIO_OUTPUT_RIGHT_ENABLE) {
        return;
    }

    fake_outputs[output] = active;
}

void fake_gpio_reset(void)
{
    for (int i = 0; i <= DESK_GPIO_INPUT_RIGHT_LIMIT_DOWN; ++i) {
        fake_inputs[i] = false;
    }

    for (int i = 0; i <= DESK_GPIO_OUTPUT_RIGHT_ENABLE; ++i) {
        fake_outputs[i] = false;
    }
}

void fake_gpio_set_input(desk_gpio_input_t input, bool active)
{
    if (input < DESK_GPIO_INPUT_BUTTON_UP || input > DESK_GPIO_INPUT_RIGHT_LIMIT_DOWN) {
        return;
    }

    fake_inputs[input] = active;
}

bool fake_gpio_get_output(desk_gpio_output_t output)
{
    if (output < DESK_GPIO_OUTPUT_LEFT_RPWM || output > DESK_GPIO_OUTPUT_RIGHT_ENABLE) {
        return false;
    }

    return fake_outputs[output];
}

const desk_gpio_hal_t *fake_gpio_hal(void)
{
    static const desk_gpio_hal_t hal = {
        .read_input = read_input,
        .set_output = set_output,
    };

    return &hal;
}
