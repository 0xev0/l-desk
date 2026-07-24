#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_network_store.h"

#define DESK_WIFI_PROVISIONING_SSID_SIZE 20U
#define DESK_WIFI_MAX_CONNECT_RETRIES 10U

typedef struct {
    esp_network_store_t *store;
    esp_netif_t *station_netif;
    esp_netif_t *access_point_netif;
    esp_event_handler_instance_t wifi_event_handler;
    esp_event_handler_instance_t ip_event_handler;
    char provisioning_ssid[DESK_WIFI_PROVISIONING_SSID_SIZE];
    uint8_t connect_retries;
    bool station_connected;
    bool provisioning_active;
    bool initialized;
} esp_wifi_service_t;

esp_err_t esp_wifi_service_init(esp_wifi_service_t *service, esp_network_store_t *store);
esp_err_t esp_wifi_service_apply_credentials(esp_wifi_service_t *service);
