#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_auth.h"
#include "desk_network_config.h"
#include "desk_preset_controller.h"
#include "esp_err.h"
#include "nvs.h"

#define DESK_PROVISIONING_AP_PASSWORD_LENGTH 12U
#define DESK_PROVISIONING_AP_PASSWORD_SIZE (DESK_PROVISIONING_AP_PASSWORD_LENGTH + 1U)

typedef struct {
    char ssid[DESK_WIFI_SSID_MAX_LENGTH + 1U];
    char password[DESK_WIFI_PASSWORD_MAX_LENGTH + 1U];
    char provisioning_ap_password[DESK_PROVISIONING_AP_PASSWORD_SIZE];
    uint8_t admin_secret[DESK_AUTH_SECRET_BYTES];
    desk_control_mode_t control_mode;
    desk_preset_bank_t presets;
    nvs_handle_t nvs;
    bool has_wifi_credentials;
    bool preset_storage_fault;
    bool initialized;
} esp_network_store_t;

esp_err_t esp_network_store_init(esp_network_store_t *store);
esp_err_t esp_network_store_save_wifi(esp_network_store_t *store, const char *ssid,
                                      size_t ssid_length, const char *password,
                                      size_t password_length);
esp_err_t esp_network_store_save_control_mode(esp_network_store_t *store, desk_control_mode_t mode);
esp_err_t esp_network_store_save_presets(esp_network_store_t *store,
                                         const desk_preset_bank_t *presets);
