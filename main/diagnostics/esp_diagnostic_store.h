#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_diagnostic_log.h"
#include "esp_err.h"
#include "nvs.h"

typedef struct {
    desk_diagnostic_log_t log;
    nvs_handle_t nvs;
    bool recovered_storage_fault;
    bool initialized;
} esp_diagnostic_store_t;

esp_err_t esp_diagnostic_store_init(esp_diagnostic_store_t *store);
esp_err_t esp_diagnostic_store_append(esp_diagnostic_store_t *store,
                                      uint32_t reset_reason,
                                      uint32_t uptime_seconds,
                                      desk_state_t state, desk_error_t error);
