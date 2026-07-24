#include "esp_network_service.h"

#include "desk_app.h"
#include "desk_auth.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DESK_NETWORK_CONTROL_READY_ATTEMPTS 100U
#define DESK_NETWORK_CONTROL_READY_DELAY_MS 10U

static const char *const TAG = "desk_network";
static esp_network_service_t network_service;

static bool apply_persisted_control_mode(desk_control_mode_t mode)
{
    for (unsigned attempt = 0U; attempt < DESK_NETWORK_CONTROL_READY_ATTEMPTS; ++attempt) {
        if (desk_app_queue_control_mode(mode)) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(DESK_NETWORK_CONTROL_READY_DELAY_MS));
    }
    return false;
}

static bool apply_persisted_presets(const desk_preset_bank_t *presets)
{
    for (size_t slot = 0U; slot < DESK_PRESET_COUNT; ++slot) {
        if (!presets->valid[slot]) {
            continue;
        }
        bool accepted = false;
        for (unsigned attempt = 0U; attempt < DESK_NETWORK_CONTROL_READY_ATTEMPTS;
             ++attempt) {
            if (desk_app_queue_preset_target(slot, presets->target_um[slot])) {
                accepted = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(DESK_NETWORK_CONTROL_READY_DELAY_MS));
        }
        if (!accepted) {
            return false;
        }
    }
    return true;
}

static void log_provisioning_credentials(const esp_network_service_t *service)
{
    char admin_secret[DESK_AUTH_SECRET_TEXT_SIZE];
    if (!desk_auth_format_secret(service->store.admin_secret, admin_secret)) {
        return;
    }
    ESP_LOGW(TAG, "provisioning SSID: %s", service->wifi.provisioning_ssid);
    ESP_LOGW(TAG, "provisioning password: %s",
             service->store.provisioning_ap_password);
    ESP_LOGW(TAG, "device administration token: %s", admin_secret);
    ESP_LOGW(TAG, "credentials are shown only on the local serial console");
}

bool esp_network_service_start(void)
{
    if (network_service.initialized) {
        return false;
    }

    esp_err_t result = esp_network_store_init(&network_service.store);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "network store unavailable: %s", esp_err_to_name(result));
        return false;
    }
    if (!apply_persisted_control_mode(network_service.store.control_mode)) {
        ESP_LOGE(TAG, "control task did not accept persisted mode");
        return false;
    }
    if (!apply_persisted_presets(&network_service.store.presets)) {
        ESP_LOGE(TAG, "control task did not accept persisted presets");
        return false;
    }
    if (network_service.store.preset_storage_fault) {
        ESP_LOGE(TAG, "corrupt preset storage ignored; all presets disabled");
    }

    result = esp_wifi_service_init(&network_service.wifi, &network_service.store);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "WiFi initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    result = esp_http_api_start(&network_service.http, &network_service.store,
                                &network_service.wifi);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "HTTP API initialization failed: %s", esp_err_to_name(result));
        return false;
    }

    network_service.initialized = true;
    if (network_service.wifi.provisioning_active) {
        log_provisioning_credentials(&network_service);
    }
    return true;
}
