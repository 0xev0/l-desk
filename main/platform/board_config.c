#include "board_config.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/gpio.h"

static const int output_pins[] = {
    PIN_AXIS_FRONT_LEFT_RPWM,
    PIN_AXIS_FRONT_LEFT_LPWM,
    PIN_AXIS_FRONT_LEFT_ENABLE,
    PIN_AXIS_FRONT_RIGHT_RPWM,
    PIN_AXIS_FRONT_RIGHT_LPWM,
    PIN_AXIS_FRONT_RIGHT_ENABLE,
    PIN_AXIS_WING_RPWM,
    PIN_AXIS_WING_LPWM,
    PIN_AXIS_WING_ENABLE,
    PIN_STATUS_LED,
};

static const int input_pins[] = {
    PIN_AXIS_FRONT_LEFT_LIMIT_UP,
    PIN_AXIS_FRONT_LEFT_LIMIT_DOWN,
    PIN_AXIS_FRONT_RIGHT_LIMIT_UP,
    PIN_AXIS_FRONT_RIGHT_LIMIT_DOWN,
    PIN_AXIS_WING_LIMIT_UP,
    PIN_AXIS_WING_LIMIT_DOWN,
    PIN_AXIS_FRONT_LEFT_HALL_A,
    PIN_AXIS_FRONT_LEFT_HALL_B,
    PIN_AXIS_FRONT_RIGHT_HALL_A,
    PIN_AXIS_FRONT_RIGHT_HALL_B,
    PIN_AXIS_WING_HALL_A,
    PIN_AXIS_WING_HALL_B,
    PIN_BUTTON_UP,
    PIN_BUTTON_DOWN,
    PIN_BUTTON_CALIBRATE,
    PIN_EMERGENCY_STOP,
};

static bool pin_is_used_more_than_once(int pin)
{
    unsigned matches = 0U;
    for (size_t index = 0U;
         index < sizeof(output_pins) / sizeof(output_pins[0]); ++index) {
        matches += output_pins[index] == pin ? 1U : 0U;
    }
    for (size_t index = 0U;
         index < sizeof(input_pins) / sizeof(input_pins[0]); ++index) {
        matches += input_pins[index] == pin ? 1U : 0U;
    }
    return matches != 1U;
}

esp_err_t board_config_validate_pins(void)
{
    for (size_t index = 0U;
         index < sizeof(output_pins) / sizeof(output_pins[0]); ++index) {
        int pin = output_pins[index];
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin) || pin_is_used_more_than_once(pin)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    for (size_t index = 0U;
         index < sizeof(input_pins) / sizeof(input_pins[0]); ++index) {
        int pin = input_pins[index];
        if (!GPIO_IS_VALID_GPIO(pin) || pin_is_used_more_than_once(pin)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}
