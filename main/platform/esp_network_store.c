#include "esp_network_store.h"

#include <stddef.h>
#include <string.h>

#include "board_config.h"
#include "desk_build_secrets.h"
#include "desk_preset_record.h"
#include "nvs_flash.h"

#define DESK_NETWORK_NVS_NAMESPACE "desk_net"
#define DESK_NETWORK_NVS_KEY_WIFI "wifi"
#define DESK_NETWORK_NVS_KEY_ADMIN "admin"
#define DESK_NETWORK_NVS_KEY_MODE "mode"
#define DESK_NETWORK_NVS_KEY_PRESETS "presets"
#define DESK_NETWORK_WIFI_RECORD_VERSION 1U

typedef struct {
    uint8_t version;
    uint8_t ssid_length;
    uint8_t password_length;
    uint8_t reserved;
    char ssid[DESK_WIFI_SSID_MAX_LENGTH + 1U];
    char password[DESK_WIFI_PASSWORD_MAX_LENGTH + 1U];
} desk_network_wifi_record_t;

static bool bytes_are_nonzero(const uint8_t *bytes, size_t length)
{
    uint8_t combined = 0U;
    for (size_t index = 0U; index < length; ++index) {
        combined |= bytes[index];
    }
    return combined != 0U;
}

static esp_err_t load_or_create_admin_secret(esp_network_store_t *store)
{
    size_t length = sizeof(store->admin_secret);
    esp_err_t result =
        nvs_get_blob(store->nvs, DESK_NETWORK_NVS_KEY_ADMIN, store->admin_secret, &length);
    if (result == ESP_OK) {
        return length == sizeof(store->admin_secret) &&
                       bytes_are_nonzero(store->admin_secret, sizeof(store->admin_secret))
                   ? ESP_OK
                   : ESP_ERR_INVALID_SIZE;
    }
    if (result != ESP_ERR_NVS_NOT_FOUND) {
        return result;
    }

    if (!desk_build_secrets_copy_admin_secret(store->admin_secret) ||
        !bytes_are_nonzero(store->admin_secret, sizeof(store->admin_secret))) {
        return ESP_FAIL;
    }
    result = nvs_set_blob(store->nvs, DESK_NETWORK_NVS_KEY_ADMIN, store->admin_secret,
                          sizeof(store->admin_secret));
    return result == ESP_OK ? nvs_commit(store->nvs) : result;
}

static void derive_provisioning_password(esp_network_store_t *store)
{
    char secret_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    (void)desk_auth_format_secret(store->admin_secret, secret_hex);
    for (size_t index = 0U; index < DESK_PROVISIONING_AP_PASSWORD_LENGTH; ++index) {
        store->provisioning_ap_password[index] = secret_hex[index];
    }
    store->provisioning_ap_password[DESK_PROVISIONING_AP_PASSWORD_LENGTH] = '\0';
}

static esp_err_t load_wifi_credentials(esp_network_store_t *store)
{
    desk_network_wifi_record_t record = {0};
    size_t record_size = sizeof(record);
    esp_err_t result = nvs_get_blob(store->nvs, DESK_NETWORK_NVS_KEY_WIFI, &record, &record_size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        store->has_wifi_credentials = false;
        return ESP_OK;
    }
    if (result != ESP_OK || record_size != sizeof(record) ||
        record.version != DESK_NETWORK_WIFI_RECORD_VERSION || record.reserved != 0U ||
        record.ssid_length > DESK_WIFI_SSID_MAX_LENGTH ||
        record.password_length > DESK_WIFI_PASSWORD_MAX_LENGTH ||
        record.ssid[record.ssid_length] != '\0' ||
        record.password[record.password_length] != '\0') {
        return result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
    }
    if (!desk_network_credentials_are_valid(record.ssid, record.ssid_length, record.password,
                                            record.password_length)) {
        return ESP_ERR_INVALID_STATE;
    }

    memcpy(store->ssid, record.ssid, sizeof(store->ssid));
    memcpy(store->password, record.password, sizeof(store->password));
    store->has_wifi_credentials = true;
    return ESP_OK;
}

static esp_err_t load_control_mode(esp_network_store_t *store)
{
    uint8_t stored_mode = (uint8_t)DESK_CONTROL_MODE_HYBRID;
    esp_err_t result = nvs_get_u8(store->nvs, DESK_NETWORK_NVS_KEY_MODE, &stored_mode);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        store->control_mode = DESK_CONTROL_MODE_HYBRID;
        return ESP_OK;
    }
    if (result != ESP_OK || !desk_network_control_mode_is_valid((desk_control_mode_t)stored_mode)) {
        return result == ESP_OK ? ESP_ERR_INVALID_STATE : result;
    }
    store->control_mode = (desk_control_mode_t)stored_mode;
    return ESP_OK;
}

static esp_err_t load_presets(esp_network_store_t *store)
{
    size_t record_size = 0U;
    esp_err_t result = nvs_get_blob(store->nvs, DESK_NETWORK_NVS_KEY_PRESETS, NULL, &record_size);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (result != ESP_OK) {
        return result;
    }
    if (record_size != DESK_PRESET_RECORD_SIZE) {
        store->preset_storage_fault = true;
        return ESP_OK;
    }

    uint8_t record[DESK_PRESET_RECORD_SIZE];
    result = nvs_get_blob(store->nvs, DESK_NETWORK_NVS_KEY_PRESETS, record, &record_size);
    if (result != ESP_OK) {
        return result;
    }
    if (!desk_preset_record_decode(record, record_size, (int32_t)BOARD_ACTUATOR_TRAVEL_UM,
                                   &store->presets)) {
        store->presets = (desk_preset_bank_t){0};
        store->preset_storage_fault = true;
    }
    return ESP_OK;
}

esp_err_t esp_network_store_init(esp_network_store_t *store)
{
    if (store == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *store = (esp_network_store_t){0};

    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) {
        return result;
    }
    result = nvs_open(DESK_NETWORK_NVS_NAMESPACE, NVS_READWRITE, &store->nvs);
    if (result == ESP_OK) {
        result = load_or_create_admin_secret(store);
    }
    if (result == ESP_OK) {
        derive_provisioning_password(store);
        result = load_wifi_credentials(store);
    }
    if (result == ESP_OK) {
        result = load_control_mode(store);
    }
    if (result == ESP_OK) {
        result = load_presets(store);
    }
    if (result != ESP_OK) {
        if (store->nvs != 0U) {
            nvs_close(store->nvs);
        }
        *store = (esp_network_store_t){0};
        return result;
    }

    store->initialized = true;
    return ESP_OK;
}

esp_err_t esp_network_store_save_wifi(esp_network_store_t *store, const char *ssid,
                                      size_t ssid_length, const char *password,
                                      size_t password_length)
{
    if (store == NULL || !store->initialized ||
        !desk_network_credentials_are_valid(ssid, ssid_length, password, password_length)) {
        return ESP_ERR_INVALID_ARG;
    }

    desk_network_wifi_record_t record = {
        .version = DESK_NETWORK_WIFI_RECORD_VERSION,
        .ssid_length = (uint8_t)ssid_length,
        .password_length = (uint8_t)password_length,
    };
    memcpy(record.ssid, ssid, ssid_length);
    memcpy(record.password, password, password_length);

    esp_err_t result = nvs_set_blob(store->nvs, DESK_NETWORK_NVS_KEY_WIFI, &record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(store->nvs);
    }
    if (result == ESP_OK) {
        memcpy(store->ssid, record.ssid, sizeof(store->ssid));
        memcpy(store->password, record.password, sizeof(store->password));
        store->has_wifi_credentials = true;
    }
    return result;
}

esp_err_t esp_network_store_save_presets(esp_network_store_t *store,
                                         const desk_preset_bank_t *presets)
{
    uint8_t record[DESK_PRESET_RECORD_SIZE];
    if (store == NULL || !store->initialized ||
        !desk_preset_record_encode(presets, (int32_t)BOARD_ACTUATOR_TRAVEL_UM, record)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result =
        nvs_set_blob(store->nvs, DESK_NETWORK_NVS_KEY_PRESETS, record, sizeof(record));
    if (result == ESP_OK) {
        result = nvs_commit(store->nvs);
    }
    if (result == ESP_OK) {
        store->presets = *presets;
        store->preset_storage_fault = false;
    }
    return result;
}

esp_err_t esp_network_store_save_control_mode(esp_network_store_t *store, desk_control_mode_t mode)
{
    if (store == NULL || !store->initialized || !desk_network_control_mode_is_valid(mode)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = nvs_set_u8(store->nvs, DESK_NETWORK_NVS_KEY_MODE, (uint8_t)mode);
    if (result == ESP_OK) {
        result = nvs_commit(store->nvs);
    }
    if (result == ESP_OK) {
        store->control_mode = mode;
    }
    return result;
}
