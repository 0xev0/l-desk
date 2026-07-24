#pragma once

#include <stdbool.h>

#include "esp_http_api.h"
#include "esp_network_store.h"
#include "esp_wifi_service.h"

typedef struct {
    esp_network_store_t store;
    esp_wifi_service_t wifi;
    esp_http_api_t http;
    bool initialized;
} esp_network_service_t;

bool esp_network_service_start(void);
