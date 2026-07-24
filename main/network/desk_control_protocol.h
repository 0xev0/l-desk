#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_command_arbiter.h"

#define DESK_CONTROL_PROTOCOL_VERSION 1U
#define DESK_CONTROL_COMMAND_FRAME_SIZE 12U

typedef enum {
    DESK_PROTOCOL_PARSE_OK = 0,
    DESK_PROTOCOL_PARSE_INVALID_ARGUMENT,
    DESK_PROTOCOL_PARSE_INVALID_SIZE,
    DESK_PROTOCOL_PARSE_UNSUPPORTED_VERSION,
    DESK_PROTOCOL_PARSE_INVALID_TYPE,
    DESK_PROTOCOL_PARSE_INVALID_ACTION,
    DESK_PROTOCOL_PARSE_INVALID_SEQUENCE,
    DESK_PROTOCOL_PARSE_INVALID_TTL,
    DESK_PROTOCOL_PARSE_RESERVED_BITS,
} desk_protocol_parse_result_t;

desk_protocol_parse_result_t desk_control_protocol_parse_command(const uint8_t *frame,
                                                                 size_t frame_size,
                                                                 uint64_t lease_id,
                                                                 desk_remote_command_t *command);
