#include "esp_hall_hal.h"

#include <limits.h>
#include <stddef.h>

#include "board_config.h"
#include "driver/gpio.h"
#include "position_scale.h"

static const int hall_a_pins[DESK_AXIS_COUNT] = {
    PIN_AXIS_FRONT_LEFT_HALL_A,
    PIN_AXIS_FRONT_RIGHT_HALL_A,
    PIN_AXIS_WING_HALL_A,
};

static const int hall_b_pins[DESK_AXIS_COUNT] = {
    PIN_AXIS_FRONT_LEFT_HALL_B,
    PIN_AXIS_FRONT_RIGHT_HALL_B,
    PIN_AXIS_WING_HALL_B,
};

static const int hall_signs[DESK_AXIS_COUNT] = {
    AXIS_FRONT_LEFT_HALL_SIGN,
    AXIS_FRONT_RIGHT_HALL_SIGN,
    AXIS_WING_HALL_SIGN,
};

static const desk_position_scale_t hall_scales[DESK_AXIS_COUNT] = {
    {BOARD_ACTUATOR_TRAVEL_UM, AXIS_FRONT_LEFT_COUNTS_PER_TRAVEL},
    {BOARD_ACTUATOR_TRAVEL_UM, AXIS_FRONT_RIGHT_COUNTS_PER_TRAVEL},
    {BOARD_ACTUATOR_TRAVEL_UM, AXIS_WING_COUNTS_PER_TRAVEL},
};

static void release_resources(esp_hall_hal_t *hal)
{
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        if (hal->units[axis] != NULL) {
            (void)pcnt_unit_stop(hal->units[axis]);
            (void)pcnt_unit_disable(hal->units[axis]);
        }
        if (hal->channels_a[axis] != NULL) {
            (void)pcnt_del_channel(hal->channels_a[axis]);
        }
        if (hal->channels_b[axis] != NULL) {
            (void)pcnt_del_channel(hal->channels_b[axis]);
        }
        if (hal->units[axis] != NULL) {
            (void)pcnt_del_unit(hal->units[axis]);
        }
    }
    *hal = (esp_hall_hal_t){0};
}

static esp_err_t configure_axis(esp_hall_hal_t *hal, size_t axis)
{
    const pcnt_unit_config_t unit_config = {
        .low_limit = -30000,
        .high_limit = 30000,
        .flags.accum_count = true,
    };
    esp_err_t result = pcnt_new_unit(&unit_config, &hal->units[axis]);
    if (result != ESP_OK) {
        return result;
    }
    const pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000U,
    };
    result = pcnt_unit_set_glitch_filter(hal->units[axis], &filter_config);
    if (result != ESP_OK) {
        return result;
    }
    result = pcnt_unit_add_watch_point(hal->units[axis], unit_config.low_limit);
    if (result == ESP_OK) {
        result = pcnt_unit_add_watch_point(hal->units[axis], unit_config.high_limit);
    }
    if (result != ESP_OK) {
        return result;
    }

    const pcnt_chan_config_t channel_a_config = {
        .edge_gpio_num = hall_a_pins[axis],
        .level_gpio_num = hall_b_pins[axis],
    };
    const pcnt_chan_config_t channel_b_config = {
        .edge_gpio_num = hall_b_pins[axis],
        .level_gpio_num = hall_a_pins[axis],
    };
    result = pcnt_new_channel(hal->units[axis], &channel_a_config, &hal->channels_a[axis]);
    if (result == ESP_OK) {
        result = pcnt_new_channel(hal->units[axis], &channel_b_config, &hal->channels_b[axis]);
    }
    if (result == ESP_OK) {
        result =
            pcnt_channel_set_edge_action(hal->channels_a[axis], PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                         PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    }
    if (result == ESP_OK) {
        result =
            pcnt_channel_set_level_action(hal->channels_a[axis], PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                          PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    }
    if (result == ESP_OK) {
        result =
            pcnt_channel_set_edge_action(hal->channels_b[axis], PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                         PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    }
    if (result == ESP_OK) {
        result =
            pcnt_channel_set_level_action(hal->channels_b[axis], PCNT_CHANNEL_LEVEL_ACTION_KEEP,
                                          PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    }
    if (result == ESP_OK) {
        result = gpio_set_pull_mode((gpio_num_t)hall_a_pins[axis], GPIO_PULLUP_ONLY);
    }
    if (result == ESP_OK) {
        result = gpio_set_pull_mode((gpio_num_t)hall_b_pins[axis], GPIO_PULLUP_ONLY);
    }
    if (result == ESP_OK) {
        result = pcnt_unit_enable(hal->units[axis]);
    }
    if (result == ESP_OK) {
        result = pcnt_unit_clear_count(hal->units[axis]);
    }
    if (result == ESP_OK) {
        result = pcnt_unit_start(hal->units[axis]);
    }
    return result;
}

esp_err_t esp_hall_hal_init(esp_hall_hal_t *hal)
{
    if (hal == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *hal = (esp_hall_hal_t){0};
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        esp_err_t result = configure_axis(hal, axis);
        if (result != ESP_OK) {
            release_resources(hal);
            return result;
        }
    }
    hal->initialized = true;
    return ESP_OK;
}

esp_err_t esp_hall_hal_read(const esp_hall_hal_t *hal, desk_inputs_t *inputs)
{
    if (hal == NULL || inputs == NULL || !hal->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        int count = 0;
        esp_err_t result = pcnt_unit_get_count(hal->units[axis], &count);
        if (result != ESP_OK) {
            return result;
        }
        int64_t signed_count = (int64_t)count * hall_signs[axis];
        if (signed_count < INT32_MIN || signed_count > INT32_MAX) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        inputs->axis[axis].hall_count = (int32_t)signed_count;
        if (!desk_position_from_count(inputs->axis[axis].hall_count, &hall_scales[axis],
                                      &inputs->axis[axis].position_um)) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        inputs->axis[axis].hall_valid = true;
    }
    return ESP_OK;
}

esp_err_t esp_hall_hal_clear(esp_hall_hal_t *hal)
{
    if (hal == NULL || !hal->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t axis = 0; axis < DESK_AXIS_COUNT; ++axis) {
        esp_err_t result = pcnt_unit_clear_count(hal->units[axis]);
        if (result != ESP_OK) {
            return result;
        }
    }
    return ESP_OK;
}
