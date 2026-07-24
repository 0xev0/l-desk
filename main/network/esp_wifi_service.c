#include "esp_wifi_service.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"

static const char *const TAG = "desk_wifi";

static esp_err_t start_provisioning(esp_wifi_service_t *service)
{
    esp_err_t result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (result == ESP_OK) {
        atomic_store_explicit(&service->provisioning_active, true, memory_order_release);
        ESP_LOGW(TAG, "provisioning AP active: SSID=%s", service->provisioning_ssid);
    }
    return result;
}

static void wifi_event_handler(void *argument, esp_event_base_t event_base, int32_t event_id,
                               void *event_data)
{
    (void)event_data;
    esp_wifi_service_t *service = argument;
    if (service == NULL) {
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START &&
        atomic_load_explicit(&service->credentials_available, memory_order_acquire)) {
        esp_err_t result = esp_wifi_connect();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "initial station connect failed: %s", esp_err_to_name(result));
        }
        return;
    }
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED &&
        atomic_load_explicit(&service->credentials_available, memory_order_acquire)) {
        atomic_store_explicit(&service->station_connected, false, memory_order_release);
        uint8_t retries = atomic_load_explicit(&service->connect_retries, memory_order_relaxed);
        if (retries < DESK_WIFI_MAX_CONNECT_RETRIES) {
            atomic_store_explicit(&service->connect_retries, (uint8_t)(retries + 1U),
                                  memory_order_relaxed);
            esp_err_t result = esp_wifi_connect();
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "station reconnect failed: %s", esp_err_to_name(result));
            }
        } else if (!atomic_load_explicit(&service->provisioning_active, memory_order_acquire)) {
            esp_err_t result = start_provisioning(service);
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "provisioning fallback failed: %s", esp_err_to_name(result));
            }
        }
        return;
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        atomic_store_explicit(&service->station_connected, true, memory_order_release);
        atomic_store_explicit(&service->connect_retries, 0U, memory_order_relaxed);
        if (atomic_load_explicit(&service->provisioning_active, memory_order_acquire)) {
            esp_err_t result = esp_wifi_set_mode(WIFI_MODE_STA);
            if (result == ESP_OK) {
                atomic_store_explicit(&service->provisioning_active, false, memory_order_release);
            } else {
                ESP_LOGE(TAG, "failed to disable fallback AP: %s", esp_err_to_name(result));
            }
        }
        ESP_LOGI(TAG, "station connected");
    }
}

static esp_err_t configure_access_point(esp_wifi_service_t *service)
{
    uint8_t mac[6];
    esp_err_t result = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (result != ESP_OK) {
        return result;
    }
    int written = snprintf(service->provisioning_ssid, sizeof(service->provisioning_ssid),
                           "Desk-%02X%02X%02X", mac[3], mac[4], mac[5]);
    if (written <= 0 || (size_t)written >= sizeof(service->provisioning_ssid)) {
        return ESP_ERR_INVALID_SIZE;
    }

    wifi_config_t config = {
        .ap =
            {
                .channel = 1U,
                .authmode = WIFI_AUTH_WPA2_PSK,
                .max_connection = 2U,
                .pmf_cfg =
                    {
                        .capable = true,
                        .required = false,
                    },
            },
    };
    size_t ssid_length = strlen(service->provisioning_ssid);
    memcpy(config.ap.ssid, service->provisioning_ssid, ssid_length);
    config.ap.ssid_len = (uint8_t)ssid_length;
    memcpy(config.ap.password, service->store->provisioning_ap_password,
           DESK_PROVISIONING_AP_PASSWORD_LENGTH);
    return esp_wifi_set_config(WIFI_IF_AP, &config);
}

static esp_err_t configure_station(const esp_network_store_t *store)
{
    wifi_config_t config = {
        .sta =
            {
                .scan_method = WIFI_ALL_CHANNEL_SCAN,
                .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
                .failure_retry_cnt = 0U,
            },
    };
    size_t ssid_length = strlen(store->ssid);
    size_t password_length = strlen(store->password);
    memcpy(config.sta.ssid, store->ssid, ssid_length);
    memcpy(config.sta.password, store->password, password_length);
    return esp_wifi_set_config(WIFI_IF_STA, &config);
}

esp_err_t esp_wifi_service_init(esp_wifi_service_t *service, esp_network_store_t *store)
{
    if (service == NULL || store == NULL || !store->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    *service = (esp_wifi_service_t){
        .store = store,
    };
    atomic_init(&service->connect_retries, 0U);
    atomic_init(&service->station_connected, false);
    atomic_init(&service->provisioning_active, false);
    atomic_init(&service->credentials_available, store->has_wifi_credentials);

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK) {
        return result;
    }
    service->station_netif = esp_netif_create_default_wifi_sta();
    service->access_point_netif = esp_netif_create_default_wifi_ap();
    if (service->station_netif == NULL || service->access_point_netif == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&wifi_init);
    if (result == ESP_OK) {
        result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (result == ESP_OK) {
        result =
            esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler,
                                                service, &service->wifi_event_handler);
    }
    if (result == ESP_OK) {
        result = esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, service, &service->ip_event_handler);
    }
    if (result == ESP_OK) {
        result = configure_access_point(service);
    }
    if (result == ESP_OK && store->has_wifi_credentials) {
        result = configure_station(store);
    }
    if (result == ESP_OK) {
        wifi_mode_t mode = store->has_wifi_credentials ? WIFI_MODE_STA : WIFI_MODE_AP;
        result = esp_wifi_set_mode(mode);
        atomic_store_explicit(&service->provisioning_active, mode == WIFI_MODE_AP,
                              memory_order_release);
    }
    if (result == ESP_OK) {
        result = esp_wifi_set_ps(WIFI_PS_NONE);
    }
    if (result == ESP_OK) {
        result = esp_wifi_start();
    }
    if (result != ESP_OK) {
        return result;
    }

    service->initialized = true;
    return ESP_OK;
}

esp_err_t esp_wifi_service_apply_credentials(esp_wifi_service_t *service)
{
    if (service == NULL || !service->initialized || service->store == NULL ||
        !service->store->has_wifi_credentials) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_mode_t current_mode;
    esp_err_t result = esp_wifi_get_mode(&current_mode);
    if (result == ESP_OK) {
        result = configure_station(service->store);
    }
    if (result == ESP_OK) {
        atomic_store_explicit(&service->credentials_available, true, memory_order_release);
        result = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    if (result == ESP_OK) {
        atomic_store_explicit(&service->provisioning_active, true, memory_order_release);
        atomic_store_explicit(&service->connect_retries, 0U, memory_order_relaxed);
        if (current_mode == WIFI_MODE_APSTA || current_mode == WIFI_MODE_STA) {
            esp_err_t disconnect_result = esp_wifi_disconnect();
            if (disconnect_result != ESP_OK && disconnect_result != ESP_ERR_WIFI_NOT_CONNECT) {
                return disconnect_result;
            }
            result = esp_wifi_connect();
        }
    }
    return result;
}

bool esp_wifi_service_station_connected(const esp_wifi_service_t *service)
{
    return service != NULL &&
           atomic_load_explicit(&service->station_connected, memory_order_acquire);
}

bool esp_wifi_service_provisioning_active(const esp_wifi_service_t *service)
{
    return service != NULL &&
           atomic_load_explicit(&service->provisioning_active, memory_order_acquire);
}
