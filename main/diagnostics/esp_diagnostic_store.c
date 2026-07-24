#include "esp_diagnostic_store.h"

#include <stddef.h>

#include "nvs_flash.h"

#define DESK_DIAGNOSTIC_PARTITION "diagnostic"
#define DESK_DIAGNOSTIC_NAMESPACE "desk_diag"
#define DESK_DIAGNOSTIC_KEY "events"

static esp_err_t load_log(esp_diagnostic_store_t *store)
{
    size_t encoded_size = 0U;
    esp_err_t result =
        nvs_get_blob(store->nvs, DESK_DIAGNOSTIC_KEY, NULL, &encoded_size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        desk_diagnostic_log_init(&store->log);
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    if (encoded_size != DESK_DIAGNOSTIC_LOG_ENCODED_SIZE) {
        desk_diagnostic_log_init(&store->log);
        store->recovered_storage_fault = true;
        return ESP_OK;
    }

    uint8_t encoded[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE];
    result = nvs_get_blob(store->nvs, DESK_DIAGNOSTIC_KEY, encoded, &encoded_size);
    if (result != ESP_OK) {
        return result;
    }
    if (!desk_diagnostic_log_decode(encoded, encoded_size, &store->log)) {
        desk_diagnostic_log_init(&store->log);
        store->recovered_storage_fault = true;
    }
    return ESP_OK;
}

esp_err_t esp_diagnostic_store_init(esp_diagnostic_store_t *store)
{
    if (store == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *store = (esp_diagnostic_store_t){0};

    esp_err_t result = nvs_flash_init_partition(DESK_DIAGNOSTIC_PARTITION);
    if (result == ESP_OK) {
        result = nvs_open_from_partition(DESK_DIAGNOSTIC_PARTITION,
                                         DESK_DIAGNOSTIC_NAMESPACE, NVS_READWRITE,
                                         &store->nvs);
    }
    if (result == ESP_OK) {
        result = load_log(store);
    }
    if (result != ESP_OK) {
        if (store->nvs != 0U) {
            nvs_close(store->nvs);
        }
        *store = (esp_diagnostic_store_t){0};
        return result;
    }
    store->initialized = true;
    return ESP_OK;
}

esp_err_t esp_diagnostic_store_append(esp_diagnostic_store_t *store,
                                      uint32_t reset_reason,
                                      uint32_t uptime_seconds,
                                      desk_state_t state, desk_error_t error)
{
    if (store == NULL || !store->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    desk_diagnostic_log_t updated = store->log;
    uint8_t encoded[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE];
    if (!desk_diagnostic_log_append(&updated, reset_reason, uptime_seconds, state,
                                    error) ||
        !desk_diagnostic_log_encode(&updated, encoded)) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result =
        nvs_set_blob(store->nvs, DESK_DIAGNOSTIC_KEY, encoded, sizeof(encoded));
    if (result == ESP_OK) {
        result = nvs_commit(store->nvs);
    }
    if (result == ESP_OK) {
        store->log = updated;
    }
    return result;
}
