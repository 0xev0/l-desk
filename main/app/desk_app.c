#include "desk_app.h"

#include "desk_config.h"
#include "desk_controller.h"
#include "desk_feedback_observer.h"
#include "desk_input_filter.h"
#include "esp_gpio_hal.h"
#include "esp_log.h"
#include "esp_time_hal.h"
#include "feedback_none_source.h"

static const char *const TAG = "desk_app";

#define DESK_SAFETY_INPUT_RELEASE_DEBOUNCE_MS 50U
#define DESK_FEEDBACK_MAX_AXIS_DELTA_UNITS 10
#define DESK_FEEDBACK_MAX_POSITION_STEP_UNITS 20

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

static const char *feedback_status_name(desk_feedback_diagnostic_status_t status)
{
    switch (status) {
    case DESK_FEEDBACK_DIAGNOSTIC_OK:
        return "OK";
    case DESK_FEEDBACK_DIAGNOSTIC_MISSING:
        return "MISSING";
    case DESK_FEEDBACK_DIAGNOSTIC_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static const char *feedback_error_name(desk_feedback_error_t error)
{
    switch (error) {
    case DESK_FEEDBACK_ERROR_NONE:
        return "NONE";
    case DESK_FEEDBACK_ERROR_MISSING:
        return "MISSING";
    case DESK_FEEDBACK_ERROR_AXIS_FAULT:
        return "AXIS_FAULT";
    case DESK_FEEDBACK_ERROR_AXIS_DISAGREEMENT:
        return "AXIS_DISAGREEMENT";
    case DESK_FEEDBACK_ERROR_POSITION_JUMP:
        return "POSITION_JUMP";
    case DESK_FEEDBACK_ERROR_INVALID_INPUT:
        return "INVALID_INPUT";
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

static void log_feedback_change(const desk_feedback_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL) {
        return;
    }

    ESP_LOGI(TAG, "feedback=%s error=%s",
             feedback_status_name(diagnostics->status),
             feedback_error_name(diagnostics->error));
}

void desk_app_run(void)
{
    ESP_LOGI(TAG, "booting motorized desk controller");
    esp_gpio_hal_init();

    desk_controller_t controller;
    desk_config_t config = desk_config_default();
    desk_controller_init(&controller, &config);

    desk_input_filter_t input_filter;
    desk_input_filter_init(&input_filter, DESK_SAFETY_INPUT_RELEASE_DEBOUNCE_MS);

    desk_feedback_config_t feedback_config = {
        .max_axis_delta_units = DESK_FEEDBACK_MAX_AXIS_DELTA_UNITS,
        .max_position_step_units = DESK_FEEDBACK_MAX_POSITION_STEP_UNITS,
    };
    desk_feedback_observer_t feedback_observer;
    desk_feedback_observer_init(&feedback_observer, feedback_none_source(), feedback_config);

    desk_outputs_t last_outputs = {
        .left_motor = MOTOR_COMMAND_STOP,
        .right_motor = MOTOR_COMMAND_STOP,
        .state = DESK_STATE_IDLE,
        .error = DESK_ERROR_NONE,
    };
    desk_feedback_diagnostics_t last_feedback = {
        .status = DESK_FEEDBACK_DIAGNOSTIC_OK,
        .error = DESK_FEEDBACK_ERROR_NONE,
        .feedback_ok = true,
    };

    while (true) {
        desk_inputs_t raw_inputs = esp_gpio_hal_read_inputs();
        desk_inputs_t inputs = desk_input_filter_update(&input_filter, &raw_inputs, config.control_tick_ms);
        desk_feedback_diagnostics_t feedback = desk_feedback_observer_poll(&feedback_observer);
        desk_outputs_t outputs = desk_controller_tick(&controller, &inputs, config.control_tick_ms);
        esp_gpio_hal_apply_outputs(&outputs);

        if (outputs.state != last_outputs.state || outputs.error != last_outputs.error) {
            log_state_change(&outputs);
        }

        if (desk_feedback_diagnostics_changed(&last_feedback, &feedback)) {
            log_feedback_change(&feedback);
        }

        last_outputs = outputs;
        last_feedback = feedback;
        desk_time_delay_ms(config.control_tick_ms);
    }
}
