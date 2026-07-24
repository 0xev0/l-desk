#include "desk_control_protocol.h"
#include "test_support.h"

static void write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8U);
    bytes[1] = (uint8_t)value;
}

static void make_valid_frame(uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE], uint8_t encoded_action,
                             uint32_t sequence, uint16_t valid_for_ms)
{
    for (size_t index = 0U; index < DESK_CONTROL_COMMAND_FRAME_SIZE; ++index) {
        frame[index] = 0U;
    }
    frame[0] = DESK_CONTROL_PROTOCOL_VERSION;
    frame[1] = 1U;
    frame[2] = encoded_action;
    write_u32_be(&frame[4], sequence);
    write_u16_be(&frame[8], valid_for_ms);
}

static void every_action_decodes_with_network_byte_order(void)
{
    const desk_control_action_t expected[] = {
        DESK_CONTROL_ACTION_STOP,      DESK_CONTROL_ACTION_UP,       DESK_CONTROL_ACTION_DOWN,
        DESK_CONTROL_ACTION_CALIBRATE, DESK_CONTROL_ACTION_PRESET_1, DESK_CONTROL_ACTION_PRESET_2,
        DESK_CONTROL_ACTION_PRESET_3,
    };
    for (uint8_t encoded = 0U; encoded < 7U; ++encoded) {
        uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE];
        make_valid_frame(frame, encoded, 0x12345678U, 250U);
        desk_remote_command_t command;
        TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_OK,
                              desk_control_protocol_parse_command(frame, sizeof(frame),
                                                                  0x1122334455667788ULL, &command));
        TEST_ASSERT_EQUAL_INT(expected[encoded], command.action);
        TEST_ASSERT_EQUAL_INT(0x12345678U, command.sequence);
        TEST_ASSERT_EQUAL_INT(250U, command.valid_for_ms);
        TEST_ASSERT_TRUE(command.lease_id == 0x1122334455667788ULL);
    }
}

static void malformed_header_and_reserved_data_are_rejected(void)
{
    uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE];
    make_valid_frame(frame, 1U, 1U, 100U);
    desk_remote_command_t command;

    frame[0] = DESK_CONTROL_PROTOCOL_VERSION + 1U;
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_UNSUPPORTED_VERSION,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));
    frame[0] = DESK_CONTROL_PROTOCOL_VERSION;
    frame[1] = 2U;
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_TYPE,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));
    frame[1] = 1U;

    const size_t reserved_offsets[] = {3U, 10U, 11U};
    for (size_t index = 0U; index < sizeof(reserved_offsets) / sizeof(reserved_offsets[0]);
         ++index) {
        frame[reserved_offsets[index]] = 1U;
        TEST_ASSERT_EQUAL_INT(
            DESK_PROTOCOL_PARSE_RESERVED_BITS,
            desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));
        frame[reserved_offsets[index]] = 0U;
    }
}

static void invalid_action_sequence_and_ttl_are_rejected(void)
{
    uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE];
    desk_remote_command_t command;
    make_valid_frame(frame, 7U, 1U, 100U);
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_ACTION,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));

    make_valid_frame(frame, 1U, 0U, 100U);
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_SEQUENCE,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));

    make_valid_frame(frame, 1U, 1U, 0U);
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_TTL,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));
    write_u16_be(&frame[8], DESK_REMOTE_COMMAND_MAX_LEASE_MS + 1U);
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_TTL,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command));
}

static void size_and_pointer_errors_fail_without_partial_command(void)
{
    uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE];
    make_valid_frame(frame, 1U, 1U, 100U);
    desk_remote_command_t command = {
        .lease_id = UINT64_MAX,
        .sequence = UINT32_MAX,
        .valid_for_ms = UINT32_MAX,
        .action = DESK_CONTROL_ACTION_DOWN,
    };

    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_ARGUMENT,
                          desk_control_protocol_parse_command(NULL, sizeof(frame), 1U, &command));
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_ARGUMENT,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 0U, &command));
    TEST_ASSERT_EQUAL_INT(DESK_PROTOCOL_PARSE_INVALID_ARGUMENT,
                          desk_control_protocol_parse_command(frame, sizeof(frame), 1U, NULL));
    TEST_ASSERT_EQUAL_INT(
        DESK_PROTOCOL_PARSE_INVALID_SIZE,
        desk_control_protocol_parse_command(frame, sizeof(frame) - 1U, 1U, &command));
    TEST_ASSERT_EQUAL_INT(0U, command.lease_id);
    TEST_ASSERT_EQUAL_INT(0U, command.sequence);
}

static void every_single_byte_corruption_of_fixed_fields_is_bounded(void)
{
    uint8_t frame[DESK_CONTROL_COMMAND_FRAME_SIZE];
    make_valid_frame(frame, 1U, 1U, 100U);
    desk_remote_command_t command;

    for (size_t offset = 0U; offset < DESK_CONTROL_COMMAND_FRAME_SIZE; ++offset) {
        uint8_t original = frame[offset];
        frame[offset] ^= 0xFFU;
        desk_protocol_parse_result_t result =
            desk_control_protocol_parse_command(frame, sizeof(frame), 1U, &command);
        TEST_ASSERT_TRUE(result >= DESK_PROTOCOL_PARSE_OK);
        TEST_ASSERT_TRUE(result <= DESK_PROTOCOL_PARSE_RESERVED_BITS);
        frame[offset] = original;
    }
}

int main(void)
{
    every_action_decodes_with_network_byte_order();
    malformed_header_and_reserved_data_are_rejected();
    invalid_action_sequence_and_ttl_are_rejected();
    size_and_pointer_errors_fail_without_partial_command();
    every_single_byte_corruption_of_fixed_fields_is_bounded();
    return 0;
}
