#include "position_scale.h"

#include <limits.h>
#include <stddef.h>

bool desk_position_scale_is_valid(const desk_position_scale_t *scale)
{
    return scale != NULL && scale->travel_um > 0U && scale->counts_per_travel > 0U;
}

bool desk_position_from_count(int32_t count, const desk_position_scale_t *scale,
                              int32_t *position_um)
{
    if (!desk_position_scale_is_valid(scale) || position_um == NULL) {
        return false;
    }

    int64_t scaled = ((int64_t)count * scale->travel_um) / scale->counts_per_travel;
    if (scaled < INT32_MIN || scaled > INT32_MAX) {
        return false;
    }

    *position_um = (int32_t)scaled;
    return true;
}
