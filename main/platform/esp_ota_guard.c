#include "esp_ota_guard.h"

#include "desk_app.h"
#include "desk_boot_health.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DESK_OTA_HEALTH_POLL_MS 10U

static const char *const TAG = "desk_ota_guard";

esp_err_t esp_ota_guard_confirm_boot_ready(uint32_t timeout_ms, bool network_service_ready)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    esp_err_t result = esp_ota_get_state_partition(running, &state);
    if (result == ESP_ERR_NOT_FOUND || result == ESP_ERR_NOT_SUPPORTED) {
        return ESP_OK;
    }
    if (result != ESP_OK || state != ESP_OTA_IMG_PENDING_VERIFY) {
        return result;
    }
    if (!network_service_ready) {
        ESP_LOGE(TAG, "network service self-test failed; requesting rollback");
        return esp_ota_mark_app_invalid_rollback_and_reboot();
    }

    uint32_t attempts = timeout_ms / DESK_OTA_HEALTH_POLL_MS;
    for (uint32_t attempt = 0U; attempt <= attempts; ++attempt) {
        desk_app_status_t status;
        if (desk_app_get_runtime_status(&status) && desk_boot_status_is_safe(&status)) {
            result = esp_ota_mark_app_valid_cancel_rollback();
            if (result == ESP_OK) {
                const esp_app_desc_t *description = esp_app_get_description();
                ESP_LOGI(TAG, "confirmed firmware %s after stopped control and network self-test",
                         description->version);
            }
            return result;
        }
        if (attempt < attempts) {
            vTaskDelay(pdMS_TO_TICKS(DESK_OTA_HEALTH_POLL_MS));
        }
    }

    ESP_LOGE(TAG, "control self-test failed; requesting rollback");
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}
