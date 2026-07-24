#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "desk_types.h"

#define DESK_DIAGNOSTIC_EVENT_COUNT 16U
#define DESK_DIAGNOSTIC_LOG_ENCODED_SIZE 332U

typedef struct {
    uint32_t sequence;
    uint32_t reset_reason;
    uint32_t uptime_seconds;
    desk_state_t state;
    desk_error_t error;
} desk_diagnostic_event_t;

typedef struct {
    desk_diagnostic_event_t events[DESK_DIAGNOSTIC_EVENT_COUNT];
    uint32_t next_sequence;
    uint8_t count;
    uint8_t next_index;
} desk_diagnostic_log_t;

void desk_diagnostic_log_init(desk_diagnostic_log_t *log);
bool desk_diagnostic_log_append(desk_diagnostic_log_t *log, uint32_t reset_reason,
                                uint32_t uptime_seconds, desk_state_t state,
                                desk_error_t error);
bool desk_diagnostic_log_get_newest(const desk_diagnostic_log_t *log, size_t offset,
                                    desk_diagnostic_event_t *event);
bool desk_diagnostic_log_encode(
    const desk_diagnostic_log_t *log,
    uint8_t encoded[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE]);
bool desk_diagnostic_log_decode(const uint8_t *encoded, size_t encoded_size,
                                desk_diagnostic_log_t *log);
