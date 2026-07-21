idf_component_register(
    SRCS
        "app_main.c"
        "app/desk_app.c"
        "app/desk_input_filter.c"
        "domain/desk_controller.c"
        "domain/motor_axis.c"
        "platform/bts7960_driver.c"
        "platform/esp_gpio_hal.c"
        "platform/esp_hall_hal.c"
        "platform/esp_time_hal.c"
    INCLUDE_DIRS
        "."
        "app"
        "domain"
        "hal"
        "platform"
)

target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_driver_ledc idf::esp_driver_pcnt)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wformat=2
    -Wshadow
)
