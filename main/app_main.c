#include "desk_app.h"
#include "esp_network_service.h"
#include "esp_ota_guard.h"

#include "esp_log.h"

#define DESK_OTA_BOOT_SELF_TEST_TIMEOUT_MS 2000U

static const char *const TAG = "desk_main";

void app_main(void)
{
    if (!desk_app_start()) {
        ESP_LOGE(TAG, "failed to start static control task; outputs remain disabled");
        return;
    }
    esp_err_t ota_result =
        esp_ota_guard_confirm_control_ready(DESK_OTA_BOOT_SELF_TEST_TIMEOUT_MS);
    if (ota_result != ESP_OK) {
        ESP_LOGE(TAG, "firmware self-test confirmation failed: %s",
                 esp_err_to_name(ota_result));
        return;
    }
    if (!esp_network_service_start()) {
        ESP_LOGE(TAG, "network unavailable; local safety control remains active");
    }
}
