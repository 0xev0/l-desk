#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

typedef struct {
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    size_t expected_size;
    size_t written_size;
    bool active;
} esp_ota_update_t;

esp_err_t esp_ota_update_begin(esp_ota_update_t *update, size_t image_size);
esp_err_t esp_ota_update_write(esp_ota_update_t *update, const void *data, size_t data_size);
esp_err_t esp_ota_update_finish(esp_ota_update_t *update);
void esp_ota_update_abort(esp_ota_update_t *update);
