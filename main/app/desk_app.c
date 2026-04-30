#include "desk_app.h"

#include "desk_config.h"
#include "desk_controller.h"
#include "esp_gpio_hal.h"
#include "esp_log.h"
#include "esp_time_hal.h"

static const char *const TAG = "desk_app";

static const char *state_name(desk_state_t state)
{
    switch (state) {
    case DESK_STATE_IDLE:
        return "IDLE";
    case DESK_STATE_MOVING_UP:
        return "MOVING_UP";
    case DESK_STATE_MOVING_DOWN:
        return "MOVING_DOWN";
    case DESK_STATE_CALIBRATING_DOWN:
        return "CALIBRATING_DOWN";
    case DESK_STATE_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char *error_name(desk_error_t error)
{
    switch (error) {
    case DESK_ERROR_NONE:
        return "NONE";
    case DESK_ERROR_EMERGENCY_STOP:
        return "EMERGENCY_STOP";
    case DESK_ERROR_BOTH_BUTTONS_PRESSED:
        return "BOTH_BUTTONS_PRESSED";
    case DESK_ERROR_ALIGNMENT_TIMEOUT:
        return "ALIGNMENT_TIMEOUT";
    case DESK_ERROR_CALIBRATION_TIMEOUT:
        return "CALIBRATION_TIMEOUT";
    case DESK_ERROR_LIMIT_CONFLICT:
        return "LIMIT_CONFLICT";
    case DESK_ERROR_INVALID_TRANSITION:
        return "INVALID_TRANSITION";
    case DESK_ERROR_HARDWARE_FAULT:
        return "HARDWARE_FAULT";
    default:
        return "UNKNOWN";
    }
}

static void log_state_change(const desk_outputs_t *outputs)
{
    if (outputs == NULL) {
        return;
    }

    ESP_LOGI(TAG, "state=%s error=%s left=%d right=%d",
             state_name(outputs->state),
             error_name(outputs->error),
             (int)outputs->left_motor,
             (int)outputs->right_motor);
}

void desk_app_run(void)
{
    ESP_LOGI(TAG, "booting motorized desk controller");
    esp_gpio_hal_init();

    desk_controller_t controller;
    desk_config_t config = desk_config_default();
    desk_controller_init(&controller, &config);

    desk_outputs_t last_outputs = {
        .left_motor = MOTOR_COMMAND_STOP,
        .right_motor = MOTOR_COMMAND_STOP,
        .state = DESK_STATE_IDLE,
        .error = DESK_ERROR_NONE,
    };

    while (true) {
        desk_inputs_t inputs = esp_gpio_hal_read_inputs();
        desk_outputs_t outputs = desk_controller_tick(&controller, &inputs, config.control_tick_ms);
        esp_gpio_hal_apply_outputs(&outputs);

        if (outputs.state != last_outputs.state || outputs.error != last_outputs.error) {
            log_state_change(&outputs);
        }

        last_outputs = outputs;
        desk_time_delay_ms(config.control_tick_ms);
    }
}
