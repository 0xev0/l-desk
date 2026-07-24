#include "desk_control_protocol.h"

#include <stddef.h>

#define DESK_CONTROL_FRAME_TYPE_COMMAND 1U

static uint16_t read_u16_be(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
}

static uint32_t read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) | ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) | bytes[3];
}

static bool decode_action(uint8_t encoded, desk_control_action_t *action)
{
    switch (encoded) {
    case 0U:
        *action = DESK_CONTROL_ACTION_STOP;
        return true;
    case 1U:
        *action = DESK_CONTROL_ACTION_UP;
        return true;
    case 2U:
        *action = DESK_CONTROL_ACTION_DOWN;
        return true;
    case 3U:
        *action = DESK_CONTROL_ACTION_CALIBRATE;
        return true;
    case 4U:
        *action = DESK_CONTROL_ACTION_PRESET_1;
        return true;
    case 5U:
        *action = DESK_CONTROL_ACTION_PRESET_2;
        return true;
    case 6U:
        *action = DESK_CONTROL_ACTION_PRESET_3;
        return true;
    default:
        return false;
    }
}

desk_protocol_parse_result_t desk_control_protocol_parse_command(
    const uint8_t *frame, size_t frame_size, uint64_t lease_id,
    desk_remote_command_t *command)
{
    if (frame == NULL || command == NULL || lease_id == 0U) {
        return DESK_PROTOCOL_PARSE_INVALID_ARGUMENT;
    }
    *command = (desk_remote_command_t){0};
    if (frame_size != DESK_CONTROL_COMMAND_FRAME_SIZE) {
        return DESK_PROTOCOL_PARSE_INVALID_SIZE;
    }
    if (frame[0] != DESK_CONTROL_PROTOCOL_VERSION) {
        return DESK_PROTOCOL_PARSE_UNSUPPORTED_VERSION;
    }
    if (frame[1] != DESK_CONTROL_FRAME_TYPE_COMMAND) {
        return DESK_PROTOCOL_PARSE_INVALID_TYPE;
    }
    if (frame[3] != 0U || frame[10] != 0U || frame[11] != 0U) {
        return DESK_PROTOCOL_PARSE_RESERVED_BITS;
    }

    desk_control_action_t action;
    if (!decode_action(frame[2], &action)) {
        return DESK_PROTOCOL_PARSE_INVALID_ACTION;
    }
    uint32_t sequence = read_u32_be(&frame[4]);
    if (sequence == 0U) {
        return DESK_PROTOCOL_PARSE_INVALID_SEQUENCE;
    }
    uint16_t valid_for_ms = read_u16_be(&frame[8]);
    if (valid_for_ms == 0U || valid_for_ms > DESK_REMOTE_COMMAND_MAX_LEASE_MS) {
        return DESK_PROTOCOL_PARSE_INVALID_TTL;
    }

    *command = (desk_remote_command_t){
        .lease_id = lease_id,
        .sequence = sequence,
        .valid_for_ms = valid_for_ms,
        .action = action,
    };
    return DESK_PROTOCOL_PARSE_OK;
}
