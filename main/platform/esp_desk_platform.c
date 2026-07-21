#include "esp_desk_platform.h"

#include <stddef.h>

#include "esp_gpio_hal.h"
#include "esp_hall_hal.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_time_hal.h"

static const char *const TAG = "desk_platform";

static const char *state_name(desk_state_t state)
{
    switch (state) {
    case DESK_STATE_SAFE_START:
        return "SAFE_START";
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

static bool initialize(void *context)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL) {
        return false;
    }
    *platform = (esp_desk_platform_context_t){0};

    esp_err_t result = esp_gpio_hal_init(&platform->gpio);
    if (result == ESP_OK) {
        result = esp_hall_hal_init(&platform->hall);
    }
    if (result == ESP_OK) {
        result = esp_task_wdt_add(NULL);
    }
    if (result != ESP_OK) {
        (void)esp_gpio_hal_stop_all(&platform->gpio);
        ESP_LOGE(TAG, "platform init failed: %s", esp_err_to_name(result));
        return false;
    }

    platform->watchdog_registered = true;
    ESP_LOGI(TAG, "three-axis platform initialized; motor outputs disabled");
    return true;
}

static bool read_inputs(void *context, desk_inputs_t *inputs)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL || inputs == NULL) {
        return false;
    }
    *inputs = (desk_inputs_t){0};
    esp_err_t result = esp_gpio_hal_read_controls(&platform->gpio, inputs);
    if (result == ESP_OK) {
        result = esp_hall_hal_read(&platform->hall, inputs);
    }
    if (result != ESP_OK) {
        inputs->hardware_fault = true;
        ESP_LOGE(TAG, "input read failed: %s", esp_err_to_name(result));
    }
    return true;
}

static bool apply_outputs(void *context, const desk_outputs_t *outputs)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL || outputs == NULL) {
        return false;
    }
    esp_err_t result = esp_gpio_hal_apply_outputs(&platform->gpio, outputs);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "motor output failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

static bool zero_hall_counts(void *context)
{
    esp_desk_platform_context_t *platform = context;
    return platform != NULL && esp_hall_hal_clear(&platform->hall) == ESP_OK;
}

static bool service_watchdog(void *context)
{
    esp_desk_platform_context_t *platform = context;
    return platform != NULL && platform->watchdog_registered && esp_task_wdt_reset() == ESP_OK;
}

static uint64_t now_ms(void *context)
{
    (void)context;
    return esp_time_hal_now_ms();
}

static void delay_ms(void *context, uint32_t delay)
{
    (void)context;
    esp_time_hal_delay_ms(delay);
}

static void log_transition(void *context, const desk_outputs_t *previous,
                           const desk_outputs_t *current)
{
    (void)context;
    (void)previous;
    if (current == NULL) {
        return;
    }
    ESP_LOGI(TAG, "state=%s error=%d warning=%d motors=[%d:%u,%d:%u,%d:%u]",
             state_name(current->state), (int)current->error, current->feedback_warning,
             (int)current->motor[0].direction, current->motor[0].duty_permille,
             (int)current->motor[1].direction, current->motor[1].duty_permille,
             (int)current->motor[2].direction, current->motor[2].duty_permille);
}

desk_platform_t esp_desk_platform_create(esp_desk_platform_context_t *context)
{
    return (desk_platform_t){
        .initialize = initialize,
        .read_inputs = read_inputs,
        .apply_outputs = apply_outputs,
        .zero_hall_counts = zero_hall_counts,
        .service_watchdog = service_watchdog,
        .now_ms = now_ms,
        .delay_ms = delay_ms,
        .log_transition = log_transition,
        .context = context,
    };
}
