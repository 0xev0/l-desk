#include "bts7960_driver.h"

#include <stddef.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "motor_axis.h"

static esp_err_t set_pwm(ledc_channel_t channel, uint32_t duty)
{
    esp_err_t result = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    return result == ESP_OK ? ledc_update_duty(LEDC_LOW_SPEED_MODE, channel) : result;
}

static esp_err_t stop_outputs(bts7960_driver_t *driver)
{
    esp_err_t first_error = gpio_set_level((gpio_num_t)driver->enable_pin, 0);
    esp_err_t result = set_pwm(driver->rpwm_channel, 0U);
    if (first_error == ESP_OK && result != ESP_OK) {
        first_error = result;
    }
    result = set_pwm(driver->lpwm_channel, 0U);
    if (first_error == ESP_OK && result != ESP_OK) {
        first_error = result;
    }
    driver->applied_direction = MOTOR_COMMAND_STOP;
    return first_error;
}

esp_err_t bts7960_driver_init(bts7960_driver_t *driver)
{
    if (driver == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = gpio_set_level((gpio_num_t)driver->enable_pin, 0);
    if (result != ESP_OK) {
        return result;
    }
    gpio_config_t enable_config = {
        .pin_bit_mask = 1ULL << driver->enable_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    result = gpio_config(&enable_config);
    if (result != ESP_OK) {
        return result;
    }

    const ledc_channel_config_t channels[] = {
        {
            .gpio_num = driver->rpwm_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = driver->rpwm_channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0U,
            .hpoint = 0U,
        },
        {
            .gpio_num = driver->lpwm_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = driver->lpwm_channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0U,
            .hpoint = 0U,
        },
    };
    for (size_t channel = 0; channel < sizeof(channels) / sizeof(channels[0]); ++channel) {
        result = ledc_channel_config(&channels[channel]);
        if (result != ESP_OK) {
            return result;
        }
    }

    driver->initialized = true;
    driver->applied_direction = MOTOR_COMMAND_STOP;
    return stop_outputs(driver);
}

esp_err_t bts7960_driver_stop(bts7960_driver_t *driver)
{
    if (driver == NULL || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return stop_outputs(driver);
}

esp_err_t bts7960_driver_apply(bts7960_driver_t *driver,
                               const desk_axis_output_t *command)
{
    if (driver == NULL || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!motor_axis_command_is_valid(command)) {
        (void)stop_outputs(driver);
        return ESP_ERR_INVALID_ARG;
    }
    if (command->direction == MOTOR_COMMAND_STOP) {
        return stop_outputs(driver);
    }

    if (driver->applied_direction != command->direction) {
        esp_err_t result = stop_outputs(driver);
        if (result != ESP_OK) {
            return result;
        }
    }

    uint32_t duty = ((uint32_t)command->duty_permille * MOTOR_PWM_MAX_DUTY) / 1000U;
    ledc_channel_t active_channel = command->direction == MOTOR_COMMAND_UP
                                        ? driver->rpwm_channel
                                        : driver->lpwm_channel;
    ledc_channel_t inactive_channel = command->direction == MOTOR_COMMAND_UP
                                          ? driver->lpwm_channel
                                          : driver->rpwm_channel;
    esp_err_t result = set_pwm(inactive_channel, 0U);
    if (result == ESP_OK) {
        result = set_pwm(active_channel, duty);
    }
    if (result == ESP_OK) {
        result = gpio_set_level((gpio_num_t)driver->enable_pin, 1);
    }
    if (result != ESP_OK) {
        (void)stop_outputs(driver);
        return result;
    }

    driver->applied_direction = command->direction;
    return ESP_OK;
}
