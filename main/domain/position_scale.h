#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t travel_um;
    uint32_t counts_per_travel;
} desk_position_scale_t;

bool desk_position_scale_is_valid(const desk_position_scale_t *scale);
bool desk_position_from_count(int32_t count, const desk_position_scale_t *scale,
                              int32_t *position_um);
