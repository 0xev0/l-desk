#include "desk_app.h"

#include <stdatomic.h>

#include "board_config.h"
#include "esp_desk_platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef enum {
    DESK_RUNTIME_REQUEST_REMOTE_LEASE = 0,
    DESK_RUNTIME_REQUEST_REMOTE_RELEASE,
    DESK_RUNTIME_REQUEST_REMOTE_COMMAND,
    DESK_RUNTIME_REQUEST_CONTROL_MODE,
} desk_runtime_request_kind_t;

typedef struct {
    desk_runtime_request_kind_t kind;
    uint64_t remote_lease_id;
    desk_remote_command_t remote_command;
    desk_control_mode_t control_mode;
} desk_runtime_request_t;

typedef struct {
    desk_app_t app;
    desk_config_t config;
    esp_desk_platform_context_t platform_context;
    desk_platform_t platform;
    StaticTask_t task_storage;
    StackType_t task_stack[BOARD_CONTROL_TASK_STACK_WORDS];
    TaskHandle_t task_handle;
    QueueHandle_t request_queue;
    StaticQueue_t request_queue_storage;
    uint8_t request_queue_buffer[BOARD_CONTROL_REQUEST_QUEUE_DEPTH *
                                 sizeof(desk_runtime_request_t)];
    desk_app_status_t status;
    bool status_valid;
    atomic_bool ready;
} desk_app_runtime_t;

static desk_app_runtime_t runtime;
static portMUX_TYPE runtime_status_lock = portMUX_INITIALIZER_UNLOCKED;

static void publish_status(desk_app_runtime_t *desk_runtime)
{
    desk_app_status_t status;
    if (!desk_app_get_status(&desk_runtime->app, &status)) {
        return;
    }
    taskENTER_CRITICAL(&runtime_status_lock);
    desk_runtime->status = status;
    desk_runtime->status_valid = true;
    taskEXIT_CRITICAL(&runtime_status_lock);
}

static void process_control_requests(desk_app_runtime_t *desk_runtime)
{
    for (size_t index = 0U; index < BOARD_CONTROL_REQUEST_QUEUE_DEPTH; ++index) {
        desk_runtime_request_t request;
        if (xQueueReceive(desk_runtime->request_queue, &request, 0U) != pdTRUE) {
            return;
        }
        switch (request.kind) {
        case DESK_RUNTIME_REQUEST_REMOTE_LEASE:
            (void)desk_app_acquire_remote_lease(&desk_runtime->app, request.remote_lease_id);
            break;
        case DESK_RUNTIME_REQUEST_REMOTE_RELEASE:
            (void)desk_app_release_remote_lease(&desk_runtime->app, request.remote_lease_id);
            break;
        case DESK_RUNTIME_REQUEST_REMOTE_COMMAND:
            (void)desk_app_submit_remote_command(&desk_runtime->app, &request.remote_command);
            break;
        case DESK_RUNTIME_REQUEST_CONTROL_MODE:
            (void)desk_app_set_control_mode(&desk_runtime->app, request.control_mode);
            break;
        }
    }
}

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
        process_control_requests(desk_runtime);
        (void)desk_app_step(&desk_runtime->app);
        publish_status(desk_runtime);
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
    runtime.request_queue = xQueueCreateStatic(
        BOARD_CONTROL_REQUEST_QUEUE_DEPTH, sizeof(desk_runtime_request_t),
        runtime.request_queue_buffer, &runtime.request_queue_storage);
    if (runtime.request_queue == NULL) {
        return false;
    }

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

bool desk_app_get_runtime_status(desk_app_status_t *status)
{
    if (status == NULL ||
        !atomic_load_explicit(&runtime.ready, memory_order_acquire)) {
        return false;
    }

    bool valid;
    taskENTER_CRITICAL(&runtime_status_lock);
    valid = runtime.status_valid;
    if (valid) {
        *status = runtime.status;
    }
    taskEXIT_CRITICAL(&runtime_status_lock);
    return valid;
}

static bool queue_runtime_request(const desk_runtime_request_t *request)
{
    return request != NULL && runtime.request_queue != NULL &&
           atomic_load_explicit(&runtime.ready, memory_order_acquire) &&
           xQueueSend(runtime.request_queue, request, 0U) == pdTRUE;
}

bool desk_app_queue_remote_command(const desk_remote_command_t *command)
{
    if (command == NULL) {
        return false;
    }
    return queue_runtime_request(&(desk_runtime_request_t){
        .kind = DESK_RUNTIME_REQUEST_REMOTE_COMMAND,
        .remote_command = *command,
    });
}

bool desk_app_queue_remote_lease(uint64_t lease_id)
{
    return queue_runtime_request(&(desk_runtime_request_t){
        .kind = DESK_RUNTIME_REQUEST_REMOTE_LEASE,
        .remote_lease_id = lease_id,
    });
}

bool desk_app_queue_remote_release(uint64_t lease_id)
{
    return queue_runtime_request(&(desk_runtime_request_t){
        .kind = DESK_RUNTIME_REQUEST_REMOTE_RELEASE,
        .remote_lease_id = lease_id,
    });
}

bool desk_app_queue_control_mode(desk_control_mode_t mode)
{
    return queue_runtime_request(&(desk_runtime_request_t){
        .kind = DESK_RUNTIME_REQUEST_CONTROL_MODE,
        .control_mode = mode,
    });
}
