#include "esp_desk_platform.h"

#include <stddef.h>

#include "esp_gpio_hal.h"
#include "esp_hall_hal.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_time_hal.h"

static const char *const TAG = "desk_platform";

static void persist_error_transition(esp_desk_platform_context_t *platform,
                                     const desk_outputs_t *outputs)
{
    if (outputs->error == DESK_ERROR_NONE ||
        !platform->diagnostic_store.initialized ||
        platform->diagnostic_mutex == NULL ||
        xSemaphoreTake(platform->diagnostic_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return;
    }
    esp_err_t result = esp_diagnostic_store_append(
        &platform->diagnostic_store, 0U,
        (uint32_t)(esp_time_hal_now_ms() / 1000U), outputs->state, outputs->error);
    (void)xSemaphoreGive(platform->diagnostic_mutex);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "failed to persist error history: %s", esp_err_to_name(result));
    }
}

static const char *log_kind_name(esp_desk_log_kind_t kind)
{
    switch (kind) {
    case ESP_DESK_LOG_INITIALIZED:
        return "initialized";
    case ESP_DESK_LOG_TRANSITION:
        return "transition";
    case ESP_DESK_LOG_INPUT_ERROR:
        return "input";
    case ESP_DESK_LOG_OUTPUT_ERROR:
        return "output";
    case ESP_DESK_LOG_HALL_ZERO_ERROR:
        return "hall_zero";
    case ESP_DESK_LOG_WATCHDOG_ERROR:
        return "watchdog";
    }
    return "unknown";
}

static const char *state_name(desk_state_t state)
{
    switch (state) {
    case DESK_STATE_SAFE_START:
        return "SAFE_START";
    case DESK_STATE_UNREFERENCED:
        return "UNREFERENCED";
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

static bool enqueue_log(esp_desk_platform_context_t *platform,
                        const esp_desk_log_record_t *record)
{
    if (platform == NULL || platform->log_queue == NULL || record == NULL) {
        return false;
    }
    if (xQueueSend(platform->log_queue, record, 0U) == pdTRUE) {
        return true;
    }
    (void)atomic_fetch_add_explicit(&platform->dropped_log_records, 1U, memory_order_relaxed);
    return false;
}

static void logger_task(void *argument)
{
    esp_desk_platform_context_t *platform = argument;
    esp_desk_log_record_t record;

    while (true) {
        if (xQueueReceive(platform->log_queue, &record, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (record.kind == ESP_DESK_LOG_INITIALIZED) {
            ESP_LOGI(TAG, "three-axis platform initialized; motor outputs disabled");
        } else if (record.kind == ESP_DESK_LOG_TRANSITION) {
            const desk_outputs_t *outputs = &record.outputs;
            persist_error_transition(platform, outputs);
            ESP_LOGI(
                TAG, "state=%s error=%d warning=%d calibrated=%d motors=[%d:%u,%d:%u,%d:%u]",
                state_name(outputs->state), (int)outputs->error, outputs->feedback_warning,
                outputs->calibration_completed, (int)outputs->motor[0].direction,
                outputs->motor[0].duty_permille, (int)outputs->motor[1].direction,
                outputs->motor[1].duty_permille, (int)outputs->motor[2].direction,
                outputs->motor[2].duty_permille);
        } else {
            ESP_LOGE(TAG, "%s operation failed: %s", log_kind_name(record.kind),
                     esp_err_to_name(record.error));
        }
    }
}

static esp_err_t initialize_logging(esp_desk_platform_context_t *platform)
{
    platform->log_queue =
        xQueueCreateStatic(BOARD_LOG_QUEUE_DEPTH, sizeof(esp_desk_log_record_t),
                           platform->log_queue_buffer, &platform->log_queue_storage);
    if (platform->log_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    platform->log_task = xTaskCreateStaticPinnedToCore(
        logger_task, "desk_log", BOARD_LOG_TASK_STACK_WORDS, platform, BOARD_LOG_TASK_PRIORITY,
        platform->log_task_stack, &platform->log_task_storage, BOARD_LOG_TASK_CORE);
    return platform->log_task != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool initialize(void *context)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL) {
        return false;
    }
    *platform = (esp_desk_platform_context_t){0};
    atomic_init(&platform->dropped_log_records, 0U);

    platform->diagnostic_mutex =
        xSemaphoreCreateMutexStatic(&platform->diagnostic_mutex_storage);
    esp_err_t diagnostic_result =
        platform->diagnostic_mutex != NULL
            ? esp_diagnostic_store_init(&platform->diagnostic_store)
            : ESP_ERR_NO_MEM;
    if (diagnostic_result == ESP_OK) {
        diagnostic_result = esp_diagnostic_store_append(
            &platform->diagnostic_store, (uint32_t)esp_reset_reason(), 0U,
            DESK_STATE_SAFE_START, DESK_ERROR_NONE);
    }
    if (diagnostic_result != ESP_OK) {
        ESP_LOGE(TAG, "persistent diagnostics unavailable: %s",
                 esp_err_to_name(diagnostic_result));
    }

    esp_err_t result = esp_gpio_hal_init(&platform->gpio);
    if (result == ESP_OK) {
        result = esp_hall_hal_init(&platform->hall);
    }
    if (result == ESP_OK) {
        result = esp_task_wdt_add(NULL);
    }
    if (result == ESP_OK) {
        platform->watchdog_registered = true;
        result = initialize_logging(platform);
    }
    if (result != ESP_OK) {
        (void)esp_gpio_hal_stop_all(&platform->gpio);
        if (platform->watchdog_registered) {
            (void)esp_task_wdt_delete(NULL);
            platform->watchdog_registered = false;
        }
        ESP_LOGE(TAG, "platform init failed: %s", esp_err_to_name(result));
        return false;
    }

    (void)enqueue_log(platform,
                      &(esp_desk_log_record_t){
                          .kind = ESP_DESK_LOG_INITIALIZED,
                      });
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
        (void)enqueue_log(platform,
                          &(esp_desk_log_record_t){
                              .kind = ESP_DESK_LOG_INPUT_ERROR,
                              .error = result,
                          });
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
        (void)enqueue_log(platform,
                          &(esp_desk_log_record_t){
                              .kind = ESP_DESK_LOG_OUTPUT_ERROR,
                              .error = result,
                          });
        return false;
    }
    return true;
}

static bool zero_hall_counts(void *context)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL) {
        return false;
    }
    esp_err_t result = esp_hall_hal_clear(&platform->hall);
    if (result != ESP_OK) {
        (void)enqueue_log(platform,
                          &(esp_desk_log_record_t){
                              .kind = ESP_DESK_LOG_HALL_ZERO_ERROR,
                              .error = result,
                          });
    }
    return result == ESP_OK;
}

static bool service_watchdog(void *context)
{
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL || !platform->watchdog_registered) {
        return false;
    }
    esp_err_t result = esp_task_wdt_reset();
    if (result != ESP_OK) {
        (void)enqueue_log(platform,
                          &(esp_desk_log_record_t){
                              .kind = ESP_DESK_LOG_WATCHDOG_ERROR,
                              .error = result,
                          });
    }
    return result == ESP_OK;
}

static uint64_t now_ms(void *context)
{
    (void)context;
    return esp_time_hal_now_ms();
}

static void log_transition(void *context, const desk_outputs_t *previous,
                           const desk_outputs_t *current)
{
    (void)previous;
    esp_desk_platform_context_t *platform = context;
    if (platform == NULL || current == NULL) {
        return;
    }
    (void)enqueue_log(platform,
                      &(esp_desk_log_record_t){
                          .kind = ESP_DESK_LOG_TRANSITION,
                          .outputs = *current,
                      });
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
        .log_transition = log_transition,
        .context = context,
    };
}

bool esp_desk_platform_get_diagnostics(const esp_desk_platform_context_t *context,
                                       esp_desk_platform_diagnostics_t *diagnostics)
{
    if (context == NULL || diagnostics == NULL) {
        return false;
    }
    *diagnostics = (esp_desk_platform_diagnostics_t){
        .current_free_heap_bytes = esp_get_free_heap_size(),
        .minimum_free_heap_bytes = esp_get_minimum_free_heap_size(),
        .dropped_log_records =
            (uint32_t)atomic_load_explicit(&context->dropped_log_records, memory_order_relaxed),
        .logger_stack_high_water_words =
            context->log_task != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(context->log_task)
                                      : 0U,
    };
    return true;
}

bool esp_desk_platform_get_diagnostic_log(esp_desk_platform_context_t *context,
                                          desk_diagnostic_log_t *log,
                                          bool *recovered_storage_fault)
{
    if (context == NULL || log == NULL || recovered_storage_fault == NULL ||
        !context->diagnostic_store.initialized || context->diagnostic_mutex == NULL ||
        xSemaphoreTake(context->diagnostic_mutex, pdMS_TO_TICKS(100U)) != pdTRUE) {
        return false;
    }
    *log = context->diagnostic_store.log;
    *recovered_storage_fault =
        context->diagnostic_store.recovered_storage_fault;
    (void)xSemaphoreGive(context->diagnostic_mutex);
    return true;
}
