#include "desk_preset_record.h"
#include "test_support.h"

#include <string.h>

static void record_round_trips_all_slots(void)
{
    desk_preset_bank_t source = {0};
    TEST_ASSERT_TRUE(desk_preset_bank_set(&source, 0U, 0, 500000));
    TEST_ASSERT_TRUE(desk_preset_bank_set(&source, 1U, 250000, 500000));
    TEST_ASSERT_TRUE(desk_preset_bank_set(&source, 2U, 500000, 500000));
    uint8_t record[DESK_PRESET_RECORD_SIZE];
    TEST_ASSERT_TRUE(desk_preset_record_encode(&source, 500000, record));

    desk_preset_bank_t decoded = {0};
    TEST_ASSERT_TRUE(
        desk_preset_record_decode(record, sizeof(record), 500000, &decoded));
    TEST_ASSERT_EQUAL_INT(0, decoded.target_um[0]);
    TEST_ASSERT_EQUAL_INT(250000, decoded.target_um[1]);
    TEST_ASSERT_EQUAL_INT(500000, decoded.target_um[2]);
    TEST_ASSERT_TRUE(decoded.valid[0]);
    TEST_ASSERT_TRUE(decoded.valid[1]);
    TEST_ASSERT_TRUE(decoded.valid[2]);
}

static void empty_bank_has_a_canonical_encoding(void)
{
    desk_preset_bank_t source = {0};
    uint8_t first[DESK_PRESET_RECORD_SIZE];
    uint8_t second[DESK_PRESET_RECORD_SIZE];
    TEST_ASSERT_TRUE(desk_preset_record_encode(&source, 500000, first));
    TEST_ASSERT_TRUE(desk_preset_record_encode(&source, 500000, second));
    TEST_ASSERT_EQUAL_INT(0, memcmp(first, second, sizeof(first)));

    desk_preset_bank_t decoded = {0};
    TEST_ASSERT_TRUE(desk_preset_record_decode(first, sizeof(first), 500000, &decoded));
    for (size_t slot = 0U; slot < DESK_PRESET_COUNT; ++slot) {
        TEST_ASSERT_FALSE(decoded.valid[slot]);
        TEST_ASSERT_EQUAL_INT(0, decoded.target_um[slot]);
    }
}

static void corruption_and_noncanonical_data_are_rejected(void)
{
    desk_preset_bank_t source = {0};
    TEST_ASSERT_TRUE(desk_preset_bank_set(&source, 1U, 250000, 500000));
    uint8_t valid[DESK_PRESET_RECORD_SIZE];
    TEST_ASSERT_TRUE(desk_preset_record_encode(&source, 500000, valid));

    for (size_t byte = 0U; byte < sizeof(valid); ++byte) {
        uint8_t corrupted[DESK_PRESET_RECORD_SIZE];
        memcpy(corrupted, valid, sizeof(corrupted));
        corrupted[byte] ^= 0x01U;
        desk_preset_bank_t decoded = {0};
        TEST_ASSERT_FALSE(
            desk_preset_record_decode(corrupted, sizeof(corrupted), 500000, &decoded));
    }

    TEST_ASSERT_FALSE(desk_preset_record_decode(valid, sizeof(valid) - 1U, 500000,
                                                &(desk_preset_bank_t){0}));
    TEST_ASSERT_FALSE(desk_preset_record_decode(valid, sizeof(valid), 200000,
                                                &(desk_preset_bank_t){0}));
}

static void invalid_arguments_and_targets_are_rejected(void)
{
    desk_preset_bank_t invalid = {
        .target_um = {500001, 0, 0},
        .valid = {true, false, false},
    };
    uint8_t record[DESK_PRESET_RECORD_SIZE];
    TEST_ASSERT_FALSE(desk_preset_record_encode(NULL, 500000, record));
    TEST_ASSERT_FALSE(desk_preset_record_encode(&invalid, 500000, record));
    TEST_ASSERT_FALSE(desk_preset_record_encode(&invalid, 0, record));
    TEST_ASSERT_FALSE(desk_preset_record_encode(&invalid, 500000, NULL));
    TEST_ASSERT_FALSE(desk_preset_record_decode(NULL, sizeof(record), 500000,
                                                &(desk_preset_bank_t){0}));
    TEST_ASSERT_FALSE(
        desk_preset_record_decode(record, sizeof(record), 500000, NULL));
}

int main(void)
{
    record_round_trips_all_slots();
    empty_bank_has_a_canonical_encoding();
    corruption_and_noncanonical_data_are_rejected();
    invalid_arguments_and_targets_are_rejected();
    return 0;
}
