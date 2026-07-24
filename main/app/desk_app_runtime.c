#include "desk_app.h"

#include "board_config.h"
#include "esp_desk_platform.h"

void desk_app_run(void)
{
    desk_app_t app;
    desk_config_t config = desk_config_default();
    config.feedback_required = BOARD_HALL_FEEDBACK_REQUIRED != 0;
    config.feedback_warning_um = BOARD_HALL_WARNING_UM;
    config.feedback_stop_um = BOARD_HALL_STOP_UM;
    config.feedback_stall_timeout_ms = BOARD_HALL_STALL_TIMEOUT_MS;
    config.starting_duty_permille = BOARD_MOTOR_START_DUTY_PERMILLE;
    config.maximum_duty_permille = BOARD_MOTOR_MAX_DUTY_PERMILLE;
    config.synchronization_trim_permille = BOARD_SYNCHRONIZATION_TRIM_PERMILLE;
    esp_desk_platform_context_t platform_context;
    desk_platform_t platform = esp_desk_platform_create(&platform_context);
    if (!desk_app_init(&app, &platform, &config)) {
        return;
    }

    while (true) {
        (void)desk_app_step(&app);
        app.platform.delay_ms(app.platform.context, config.control_tick_ms);
    }
}
