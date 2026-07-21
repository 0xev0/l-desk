#include "desk_app.h"

#include "esp_desk_platform.h"

void desk_app_run(void)
{
    desk_app_t app;
    desk_config_t config = desk_config_default();
    desk_platform_t platform = esp_desk_platform();
    if (!desk_app_init(&app, &platform, &config)) {
        return;
    }

    while (true) {
        (void)desk_app_step(&app);
        app.platform.delay_ms(app.platform.context, config.control_tick_ms);
    }
}
