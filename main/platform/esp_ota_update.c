#include "esp_ota_update.h"

esp_err_t esp_ota_update_begin(esp_ota_update_t *update, size_t image_size)
{
    if (update == NULL || image_size == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    *update = (esp_ota_update_t){0};
    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL || image_size > partition->size) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_ota_handle_t handle;
    esp_err_t result = esp_ota_begin(partition, image_size, &handle);
    if (result == ESP_OK) {
        *update = (esp_ota_update_t){
            .partition = partition,
            .handle = handle,
            .expected_size = image_size,
            .active = true,
        };
    }
    return result;
}

esp_err_t esp_ota_update_write(esp_ota_update_t *update, const void *data, size_t data_size)
{
    if (update == NULL || !update->active || data == NULL || data_size == 0U ||
        update->written_size > update->expected_size ||
        data_size > update->expected_size - update->written_size) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = esp_ota_write(update->handle, data, data_size);
    if (result == ESP_OK) {
        update->written_size += data_size;
    }
    return result;
}

esp_err_t esp_ota_update_finish(esp_ota_update_t *update)
{
    if (update == NULL || !update->active || update->written_size != update->expected_size) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t result = esp_ota_end(update->handle);
    update->active = false;
    if (result == ESP_OK) {
        result = esp_ota_set_boot_partition(update->partition);
    }
    return result;
}

void esp_ota_update_abort(esp_ota_update_t *update)
{
    if (update != NULL && update->active) {
        (void)esp_ota_abort(update->handle);
        update->active = false;
    }
}
