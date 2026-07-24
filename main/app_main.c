#include "desk_app.h"

#include "esp_log.h"

static const char *const TAG = "desk_main";

void app_main(void)
{
    if (!desk_app_start()) {
        ESP_LOGE(TAG, "failed to start static control task; outputs remain disabled");
    }
}
