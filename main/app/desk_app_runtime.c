#include "desk_app.h"

#include <stdatomic.h>

#include "board_config.h"
#include "esp_desk_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

typedef struct {
    desk_app_t app;
    desk_config_t config;
    esp_desk_platform_context_t platform_context;
    desk_platform_t platform;
    StaticTask_t task_storage;
    StackType_t task_stack[BOARD_CONTROL_TASK_STACK_WORDS];
    TaskHandle_t task_handle;
    atomic_bool ready;
} desk_app_runtime_t;

static desk_app_runtime_t runtime;

static void control_task(void *argument)
{
    desk_app_runtime_t *desk_runtime = argument;
    if (desk_runtime == NULL ||
        !desk_app_init(&desk_runtime->app, &desk_runtime->platform, &desk_runtime->config)) {
        vTaskDelete(NULL);
        return;
    }

    const TickType_t period_ticks = pdMS_TO_TICKS(desk_runtime->config.control_tick_ms);
    if (period_ticks == 0U) {
        vTaskDelete(NULL);
        return;
    }
    atomic_store_explicit(&desk_runtime->ready, true, memory_order_release);
    TickType_t last_wake_tick = xTaskGetTickCount();
    while (true) {
        (void)desk_app_step(&desk_runtime->app);
        vTaskDelayUntil(&last_wake_tick, period_ticks);
    }
}

bool desk_app_start(void)
{
    if (runtime.task_handle != NULL) {
        return false;
    }

    atomic_store_explicit(&runtime.ready, false, memory_order_relaxed);
    runtime.config = desk_config_default();
    runtime.config.feedback_required = BOARD_HALL_FEEDBACK_REQUIRED != 0;
    runtime.config.feedback_warning_um = BOARD_HALL_WARNING_UM;
    runtime.config.feedback_stop_um = BOARD_HALL_STOP_UM;
    runtime.config.feedback_stall_timeout_ms = BOARD_HALL_STALL_TIMEOUT_MS;
    runtime.config.starting_duty_permille = BOARD_MOTOR_START_DUTY_PERMILLE;
    runtime.config.maximum_duty_permille = BOARD_MOTOR_MAX_DUTY_PERMILLE;
    runtime.config.synchronization_trim_permille = BOARD_SYNCHRONIZATION_TRIM_PERMILLE;
    runtime.platform = esp_desk_platform_create(&runtime.platform_context);

    runtime.task_handle = xTaskCreateStaticPinnedToCore(
        control_task, "desk_control", BOARD_CONTROL_TASK_STACK_WORDS, &runtime,
        BOARD_CONTROL_TASK_PRIORITY, runtime.task_stack, &runtime.task_storage,
        BOARD_CONTROL_TASK_CORE);
    return runtime.task_handle != NULL;
}

bool desk_app_get_runtime_diagnostics(desk_runtime_diagnostics_t *diagnostics)
{
    if (diagnostics == NULL || runtime.task_handle == NULL ||
        !atomic_load_explicit(&runtime.ready, memory_order_acquire)) {
        return false;
    }

    esp_desk_platform_diagnostics_t platform_diagnostics;
    if (!esp_desk_platform_get_diagnostics(&runtime.platform_context, &platform_diagnostics)) {
        return false;
    }
    *diagnostics = (desk_runtime_diagnostics_t){
        .current_free_heap_bytes = platform_diagnostics.current_free_heap_bytes,
        .minimum_free_heap_bytes = platform_diagnostics.minimum_free_heap_bytes,
        .dropped_log_records = platform_diagnostics.dropped_log_records,
        .control_stack_high_water_words =
            (uint32_t)uxTaskGetStackHighWaterMark(runtime.task_handle),
        .logger_stack_high_water_words = platform_diagnostics.logger_stack_high_water_words,
    };
    return true;
}
