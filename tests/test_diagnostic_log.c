#include "desk_diagnostic_log.h"
#include "test_support.h"

#include <string.h>

static void log_is_bounded_and_newest_first(void)
{
    desk_diagnostic_log_t log;
    desk_diagnostic_log_init(&log);
    for (uint32_t index = 0U; index < DESK_DIAGNOSTIC_EVENT_COUNT + 4U; ++index) {
        TEST_ASSERT_TRUE(desk_diagnostic_log_append(
            &log, index, index * 10U, DESK_STATE_ERROR,
            DESK_ERROR_HARDWARE_FAULT));
    }
    TEST_ASSERT_EQUAL_INT(DESK_DIAGNOSTIC_EVENT_COUNT, log.count);
    desk_diagnostic_event_t event;
    TEST_ASSERT_TRUE(desk_diagnostic_log_get_newest(&log, 0U, &event));
    TEST_ASSERT_EQUAL_INT(20, event.sequence);
    TEST_ASSERT_EQUAL_INT(19, event.reset_reason);
    TEST_ASSERT_TRUE(desk_diagnostic_log_get_newest(
        &log, DESK_DIAGNOSTIC_EVENT_COUNT - 1U, &event));
    TEST_ASSERT_EQUAL_INT(5, event.sequence);
    TEST_ASSERT_FALSE(
        desk_diagnostic_log_get_newest(&log, DESK_DIAGNOSTIC_EVENT_COUNT, &event));
}

static void encoded_log_round_trips_and_rejects_every_corrupt_byte(void)
{
    desk_diagnostic_log_t source;
    desk_diagnostic_log_init(&source);
    TEST_ASSERT_TRUE(desk_diagnostic_log_append(
        &source, 3U, 0U, DESK_STATE_SAFE_START, DESK_ERROR_NONE));
    TEST_ASSERT_TRUE(desk_diagnostic_log_append(
        &source, 0U, 12U, DESK_STATE_ERROR, DESK_ERROR_FEEDBACK_STALL));
    uint8_t encoded[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE];
    TEST_ASSERT_TRUE(desk_diagnostic_log_encode(&source, encoded));

    desk_diagnostic_log_t decoded;
    TEST_ASSERT_TRUE(desk_diagnostic_log_decode(encoded, sizeof(encoded), &decoded));
    desk_diagnostic_event_t newest;
    TEST_ASSERT_TRUE(desk_diagnostic_log_get_newest(&decoded, 0U, &newest));
    TEST_ASSERT_EQUAL_INT(DESK_ERROR_FEEDBACK_STALL, newest.error);

    for (size_t index = 0U; index < sizeof(encoded); ++index) {
        uint8_t corrupted[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE];
        memcpy(corrupted, encoded, sizeof(corrupted));
        corrupted[index] ^= 0x80U;
        TEST_ASSERT_FALSE(
            desk_diagnostic_log_decode(corrupted, sizeof(corrupted), &decoded));
    }
}

static void invalid_inputs_fail_without_mutating_output(void)
{
    desk_diagnostic_log_t log;
    desk_diagnostic_log_init(&log);
    TEST_ASSERT_FALSE(desk_diagnostic_log_append(
        NULL, 0U, 0U, DESK_STATE_IDLE, DESK_ERROR_NONE));
    TEST_ASSERT_FALSE(desk_diagnostic_log_append(
        &log, 0U, 0U, DESK_STATE_IDLE, DESK_ERROR_HARDWARE_FAULT));
    TEST_ASSERT_FALSE(desk_diagnostic_log_get_newest(&log, 0U,
                                                     &(desk_diagnostic_event_t){0}));
    TEST_ASSERT_FALSE(desk_diagnostic_log_encode(NULL,
                                                 (uint8_t[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE]){0}));
    TEST_ASSERT_FALSE(desk_diagnostic_log_decode(NULL, 0U, &log));
    desk_diagnostic_log_init(NULL);
}

int main(void)
{
    log_is_bounded_and_newest_first();
    encoded_log_round_trips_and_rejects_every_corrupt_byte();
    invalid_inputs_fail_without_mutating_output();
    return 0;
}
