idf_component_register(
    SRCS
        "app_main.c"
        "app/desk_app.c"
        "app/desk_feedback_observer.c"
        "app/desk_input_filter.c"
        "domain/desk_controller.c"
        "domain/desk_feedback.c"
        "domain/motor_axis.c"
        "platform/bts7960_driver.c"
        "platform/esp_gpio_hal.c"
        "platform/esp_time_hal.c"
        "platform/feedback_none_source.c"
    INCLUDE_DIRS
        "."
        "app"
        "domain"
        "hal"
        "platform"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wformat=2
    -Wshadow
)
