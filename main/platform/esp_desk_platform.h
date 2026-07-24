#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "desk_platform.h"
#include "esp_err.h"
#include "esp_gpio_hal.h"
#include "esp_hall_hal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "board_config.h"

typedef enum {
    ESP_DESK_LOG_INITIALIZED = 0,
    ESP_DESK_LOG_TRANSITION,
    ESP_DESK_LOG_INPUT_ERROR,
    ESP_DESK_LOG_OUTPUT_ERROR,
    ESP_DESK_LOG_HALL_ZERO_ERROR,
    ESP_DESK_LOG_WATCHDOG_ERROR,
} esp_desk_log_kind_t;

typedef struct {
    esp_desk_log_kind_t kind;
    esp_err_t error;
    desk_outputs_t outputs;
} esp_desk_log_record_t;

typedef struct {
    size_t current_free_heap_bytes;
    size_t minimum_free_heap_bytes;
    uint32_t dropped_log_records;
    uint32_t logger_stack_high_water_words;
} esp_desk_platform_diagnostics_t;

typedef struct {
    esp_gpio_hal_t gpio;
    esp_hall_hal_t hall;
    QueueHandle_t log_queue;
    StaticQueue_t log_queue_storage;
    uint8_t log_queue_buffer[BOARD_LOG_QUEUE_DEPTH * sizeof(esp_desk_log_record_t)];
    TaskHandle_t log_task;
    StaticTask_t log_task_storage;
    StackType_t log_task_stack[BOARD_LOG_TASK_STACK_WORDS];
    atomic_uint_least32_t dropped_log_records;
    bool watchdog_registered;
} esp_desk_platform_context_t;

desk_platform_t esp_desk_platform_create(esp_desk_platform_context_t *context);
bool esp_desk_platform_get_diagnostics(const esp_desk_platform_context_t *context,
                                       esp_desk_platform_diagnostics_t *diagnostics);
