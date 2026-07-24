#include "position_scale.h"
#include "test_support.h"

#include <limits.h>

static void converts_each_direction_without_floating_point(void)
{
    const desk_position_scale_t scale = {
        .travel_um = 500000U,
        .counts_per_travel = 10000U,
    };
    int32_t position_um = 0;

    TEST_ASSERT_TRUE(desk_position_from_count(10000, &scale, &position_um));
    TEST_ASSERT_EQUAL_INT(500000, position_um);
    TEST_ASSERT_TRUE(desk_position_from_count(-2500, &scale, &position_um));
    TEST_ASSERT_EQUAL_INT(-125000, position_um);
    TEST_ASSERT_TRUE(desk_position_from_count(1, &scale, &position_um));
    TEST_ASSERT_EQUAL_INT(50, position_um);
}

static void rejects_invalid_scales_arguments_and_overflow(void)
{
    int32_t position_um = 0;
    desk_position_scale_t scale = {
        .travel_um = UINT32_MAX,
        .counts_per_travel = 1U,
    };

    TEST_ASSERT_FALSE(desk_position_from_count(INT32_MAX, &scale, &position_um));
    TEST_ASSERT_FALSE(desk_position_from_count(0, NULL, &position_um));
    TEST_ASSERT_FALSE(desk_position_from_count(0, &scale, NULL));
    scale.travel_um = 0U;
    TEST_ASSERT_FALSE(desk_position_scale_is_valid(&scale));
    scale.travel_um = 1U;
    scale.counts_per_travel = 0U;
    TEST_ASSERT_FALSE(desk_position_scale_is_valid(&scale));
}

int main(void)
{
    converts_each_direction_without_floating_point();
    rejects_invalid_scales_arguments_and_overflow();
    return 0;
}
