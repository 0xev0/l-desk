#include "bts7960_driver.h"

#include "driver/gpio.h"
#include "motor_axis.h"

static void set_level(int pin, bool active)
{
    (void)gpio_set_level((gpio_num_t)pin, active ? 1 : 0);
}

void bts7960_driver_init(const bts7960_driver_t *driver)
{
    if (driver == NULL) {
        return;
    }

    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << driver->rpwm_pin) |
                        (1ULL << driver->lpwm_pin) |
                        (1ULL << driver->enable_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    (void)gpio_config(&output_config);
    bts7960_driver_stop(driver);
}

void bts7960_driver_apply(const bts7960_driver_t *driver, motor_command_t command)
{
    if (driver == NULL) {
        return;
    }

    motor_axis_output_t output = motor_axis_command_to_output(command);
    if (!motor_axis_command_is_valid(command) || !motor_axis_output_is_coherent(&output)) {
        output = motor_axis_command_to_output(MOTOR_COMMAND_STOP);
    }

    set_level(driver->rpwm_pin, false);
    set_level(driver->lpwm_pin, false);
    set_level(driver->enable_pin, output.enable_active);
    set_level(driver->rpwm_pin, output.rpwm_active);
    set_level(driver->lpwm_pin, output.lpwm_active);
}

void bts7960_driver_stop(const bts7960_driver_t *driver)
{
    if (driver == NULL) {
        return;
    }

    set_level(driver->rpwm_pin, false);
    set_level(driver->lpwm_pin, false);
    set_level(driver->enable_pin, false);
}
