#include "bts7960_driver.h"

#include <stddef.h>

#include "board_config.h"
#include "bts7960_sequence.h"
#include "driver/gpio.h"

static esp_err_t set_pwm(ledc_channel_t channel, uint32_t duty)
{
    esp_err_t result = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty);
    return result == ESP_OK ? ledc_update_duty(LEDC_LOW_SPEED_MODE, channel) : result;
}

static esp_err_t force_stop_outputs(bts7960_driver_t *driver)
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

static uint32_t duty_permille_to_raw(uint16_t duty_permille)
{
    return ((uint32_t)duty_permille * MOTOR_PWM_MAX_DUTY) / 1000U;
}

static esp_err_t execute_operation(bts7960_driver_t *driver, const bts7960_operation_t *operation)
{
    switch (operation->kind) {
    case BTS7960_OPERATION_SET_ENABLE:
        return gpio_set_level((gpio_num_t)driver->enable_pin, operation->value != 0U ? 1 : 0);
    case BTS7960_OPERATION_SET_RPWM:
        return set_pwm(driver->rpwm_channel, duty_permille_to_raw(operation->value));
    case BTS7960_OPERATION_SET_LPWM:
        return set_pwm(driver->lpwm_channel, duty_permille_to_raw(operation->value));
    }
    return ESP_ERR_INVALID_ARG;
}

static esp_err_t execute_sequence(bts7960_driver_t *driver, const bts7960_sequence_t *sequence)
{
    for (size_t index = 0U; index < sequence->count; ++index) {
        esp_err_t result = execute_operation(driver, &sequence->operations[index]);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
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
    return force_stop_outputs(driver);
}

esp_err_t bts7960_driver_stop(bts7960_driver_t *driver)
{
    if (driver == NULL || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return force_stop_outputs(driver);
}

esp_err_t bts7960_driver_self_check(const bts7960_driver_t *driver,
                                    const desk_axis_output_t *expected_command)
{
    if (driver == NULL || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    bts7960_snapshot_t snapshot = {
        .enable_active = gpio_get_level((gpio_num_t)driver->enable_pin) != 0,
        .rpwm_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, driver->rpwm_channel),
        .lpwm_duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, driver->lpwm_channel),
    };
    return bts7960_snapshot_matches(expected_command, &snapshot, MOTOR_PWM_MAX_DUTY) ? ESP_OK
                                                                                     : ESP_FAIL;
}

esp_err_t bts7960_driver_apply(bts7960_driver_t *driver, const desk_axis_output_t *command)
{
    if (driver == NULL || !driver->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    bts7960_sequence_t sequence;
    if (!bts7960_sequence_build(driver->applied_direction, command, &sequence)) {
        (void)force_stop_outputs(driver);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = execute_sequence(driver, &sequence);
    if (result != ESP_OK) {
        (void)force_stop_outputs(driver);
        return result;
    }

    result = bts7960_driver_self_check(driver, command);
    if (result != ESP_OK) {
        (void)force_stop_outputs(driver);
        return ESP_FAIL;
    }

    driver->applied_direction = sequence.resulting_direction;
    return ESP_OK;
}
