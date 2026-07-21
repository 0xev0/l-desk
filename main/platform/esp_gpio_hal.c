#include "esp_gpio_hal.h"

#include <stddef.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "motor_axis.h"

static const int upper_limit_pins[DESK_AXIS_COUNT] = {
    PIN_AXIS_FRONT_LEFT_LIMIT_UP,
    PIN_AXIS_FRONT_RIGHT_LIMIT_UP,
    PIN_AXIS_WING_LIMIT_UP,
};

static const int lower_limit_pins[DESK_AXIS_COUNT] = {
    PIN_AXIS_FRONT_LEFT_LIMIT_DOWN,
    PIN_AXIS_FRONT_RIGHT_LIMIT_DOWN,
    PIN_AXIS_WING_LIMIT_DOWN,
};

static bool gpio_high(int pin)
{
    return gpio_get_level((gpio_num_t)pin) != 0;
}

static esp_err_t configure_pwm_timer(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = MOTOR_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    return ledc_timer_config(&timer);
}

esp_err_t esp_gpio_hal_stop_all(esp_gpio_hal_t *hal)
{
    if (hal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t first_error = ESP_OK;
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (!hal->drivers[axis].initialized) {
            continue;
        }
        esp_err_t result = bts7960_driver_stop(&hal->drivers[axis]);
        if (first_error == ESP_OK && result != ESP_OK) {
            first_error = result;
        }
    }
    return first_error;
}

esp_err_t esp_gpio_hal_init(esp_gpio_hal_t *hal)
{
    if (hal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *hal = (esp_gpio_hal_t){
        .drivers = {
            {
                .rpwm_pin = PIN_AXIS_FRONT_LEFT_RPWM,
                .lpwm_pin = PIN_AXIS_FRONT_LEFT_LPWM,
                .enable_pin = PIN_AXIS_FRONT_LEFT_ENABLE,
                .rpwm_channel = LEDC_CHANNEL_0,
                .lpwm_channel = LEDC_CHANNEL_1,
            },
            {
                .rpwm_pin = PIN_AXIS_FRONT_RIGHT_RPWM,
                .lpwm_pin = PIN_AXIS_FRONT_RIGHT_LPWM,
                .enable_pin = PIN_AXIS_FRONT_RIGHT_ENABLE,
                .rpwm_channel = LEDC_CHANNEL_2,
                .lpwm_channel = LEDC_CHANNEL_3,
            },
            {
                .rpwm_pin = PIN_AXIS_WING_RPWM,
                .lpwm_pin = PIN_AXIS_WING_LPWM,
                .enable_pin = PIN_AXIS_WING_ENABLE,
                .rpwm_channel = LEDC_CHANNEL_4,
                .lpwm_channel = LEDC_CHANNEL_5,
            },
        },
    };

    esp_err_t result = configure_pwm_timer();
    if (result != ESP_OK) {
        return result;
    }
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        result = bts7960_driver_init(&hal->drivers[axis]);
        if (result != ESP_OK) {
            (void)esp_gpio_hal_stop_all(hal);
            return result;
        }
    }

    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_UP) |
                        (1ULL << PIN_BUTTON_DOWN) |
                        (1ULL << PIN_BUTTON_CALIBRATE) |
                        (1ULL << PIN_EMERGENCY_STOP) |
                        (1ULL << PIN_AXIS_FRONT_LEFT_LIMIT_UP) |
                        (1ULL << PIN_AXIS_FRONT_LEFT_LIMIT_DOWN) |
                        (1ULL << PIN_AXIS_FRONT_RIGHT_LIMIT_UP) |
                        (1ULL << PIN_AXIS_FRONT_RIGHT_LIMIT_DOWN) |
                        (1ULL << PIN_AXIS_WING_LIMIT_UP) |
                        (1ULL << PIN_AXIS_WING_LIMIT_DOWN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    result = gpio_config(&input_config);
    if (result != ESP_OK) {
        (void)esp_gpio_hal_stop_all(hal);
        return result;
    }

    hal->initialized = true;
    return esp_gpio_hal_stop_all(hal);
}

esp_err_t esp_gpio_hal_read_controls(const esp_gpio_hal_t *hal, desk_inputs_t *inputs)
{
    if (hal == NULL || inputs == NULL || !hal->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    inputs->up_pressed = !gpio_high(PIN_BUTTON_UP);
    inputs->down_pressed = !gpio_high(PIN_BUTTON_DOWN);
    inputs->calibrate_pressed = !gpio_high(PIN_BUTTON_CALIBRATE);
    inputs->emergency_active = gpio_high(PIN_EMERGENCY_STOP);
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        inputs->axis[axis].upper_limit_active = gpio_high(upper_limit_pins[axis]);
        inputs->axis[axis].lower_limit_active = gpio_high(lower_limit_pins[axis]);
    }
    return ESP_OK;
}

esp_err_t esp_gpio_hal_apply_outputs(esp_gpio_hal_t *hal, const desk_outputs_t *outputs)
{
    if (hal == NULL || outputs == NULL || !hal->initialized) {
        if (hal != NULL) {
            (void)esp_gpio_hal_stop_all(hal);
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (!desk_outputs_are_coherent(outputs)) {
        (void)esp_gpio_hal_stop_all(hal);
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        esp_err_t result = bts7960_driver_apply(&hal->drivers[axis], &outputs->motor[axis]);
        if (result != ESP_OK) {
            (void)esp_gpio_hal_stop_all(hal);
            return result;
        }
    }
    return ESP_OK;
}
