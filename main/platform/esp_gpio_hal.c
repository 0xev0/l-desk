#include "esp_gpio_hal.h"

#include "board_config.h"
#include "bts7960_driver.h"
#include "driver/gpio.h"

static const bts7960_driver_t left_driver = {
    .rpwm_pin = PIN_LEFT_RPWM,
    .lpwm_pin = PIN_LEFT_LPWM,
    .enable_pin = PIN_LEFT_ENABLE,
};

static const bts7960_driver_t right_driver = {
    .rpwm_pin = PIN_RIGHT_RPWM,
    .lpwm_pin = PIN_RIGHT_LPWM,
    .enable_pin = PIN_RIGHT_ENABLE,
};

static bool gpio_high(int pin)
{
    return gpio_get_level((gpio_num_t)pin) != 0;
}

static bool active_low_pressed(int pin)
{
    return !gpio_high(pin);
}

static bool nc_limit_active(int pin)
{
    return gpio_high(pin);
}

void esp_gpio_hal_init(void)
{
    bts7960_driver_init(&left_driver);
    bts7960_driver_init(&right_driver);

    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_UP) |
                        (1ULL << PIN_BUTTON_DOWN) |
                        (1ULL << PIN_BUTTON_CALIBRATE) |
                        (1ULL << PIN_EMERGENCY_STOP) |
                        (1ULL << PIN_LEFT_LIMIT_UP) |
                        (1ULL << PIN_LEFT_LIMIT_DOWN) |
                        (1ULL << PIN_RIGHT_LIMIT_UP) |
                        (1ULL << PIN_RIGHT_LIMIT_DOWN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    (void)gpio_config(&input_config);
    bts7960_driver_stop(&left_driver);
    bts7960_driver_stop(&right_driver);
}

desk_inputs_t esp_gpio_hal_read_inputs(void)
{
    return (desk_inputs_t){
        .up_pressed = active_low_pressed(PIN_BUTTON_UP),
        .down_pressed = active_low_pressed(PIN_BUTTON_DOWN),
        .calibrate_pressed = active_low_pressed(PIN_BUTTON_CALIBRATE),
        .emergency_active = gpio_high(PIN_EMERGENCY_STOP),
        .left_upper_limit_active = nc_limit_active(PIN_LEFT_LIMIT_UP),
        .left_lower_limit_active = nc_limit_active(PIN_LEFT_LIMIT_DOWN),
        .right_upper_limit_active = nc_limit_active(PIN_RIGHT_LIMIT_UP),
        .right_lower_limit_active = nc_limit_active(PIN_RIGHT_LIMIT_DOWN),
    };
}

void esp_gpio_hal_apply_outputs(const desk_outputs_t *outputs)
{
    if (outputs == NULL) {
        bts7960_driver_stop(&left_driver);
        bts7960_driver_stop(&right_driver);
        return;
    }

    bts7960_driver_apply(&left_driver, outputs->left_motor);
    bts7960_driver_apply(&right_driver, outputs->right_motor);
}
