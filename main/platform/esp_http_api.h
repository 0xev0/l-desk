#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "desk_auth.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_network_store.h"
#include "esp_wifi_service.h"

typedef struct {
    esp_network_store_t *store;
    esp_wifi_service_t *wifi;
    desk_authenticator_t authenticator;
    httpd_handle_t server;
    int websocket_fd;
    uint64_t websocket_lease_id;
    bool initialized;
} esp_http_api_t;

esp_err_t esp_http_api_start(esp_http_api_t *api, esp_network_store_t *store,
                             esp_wifi_service_t *wifi);
