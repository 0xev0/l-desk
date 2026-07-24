#include "desk_diagnostic_log.h"

#include <string.h>

#include "desk_crc32.h"

#define DESK_DIAGNOSTIC_LOG_VERSION 1U
#define DESK_DIAGNOSTIC_LOG_HEADER_SIZE 8U
#define DESK_DIAGNOSTIC_EVENT_ENCODED_SIZE 20U
#define DESK_DIAGNOSTIC_LOG_CRC_OFFSET 328U

static void write_u32_le(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
    destination[2] = (uint8_t)(value >> 16U);
    destination[3] = (uint8_t)(value >> 24U);
}

static uint32_t read_u32_le(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) | ((uint32_t)source[2] << 16U) |
           ((uint32_t)source[3] << 24U);
}

static bool event_is_valid(const desk_diagnostic_event_t *event)
{
    return event->sequence != 0U && event->state <= DESK_STATE_ERROR &&
           event->error <= DESK_ERROR_FEEDBACK_SKEW &&
           ((event->error == DESK_ERROR_NONE) || (event->state == DESK_STATE_ERROR));
}

void desk_diagnostic_log_init(desk_diagnostic_log_t *log)
{
    if (log != NULL) {
        *log = (desk_diagnostic_log_t){
            .next_sequence = 1U,
        };
    }
}

bool desk_diagnostic_log_append(desk_diagnostic_log_t *log, uint32_t reset_reason,
                                uint32_t uptime_seconds, desk_state_t state, desk_error_t error)
{
    if (log == NULL || log->count > DESK_DIAGNOSTIC_EVENT_COUNT ||
        log->next_index >= DESK_DIAGNOSTIC_EVENT_COUNT || log->next_sequence == 0U) {
        return false;
    }
    desk_diagnostic_event_t event = {
        .sequence = log->next_sequence,
        .reset_reason = reset_reason,
        .uptime_seconds = uptime_seconds,
        .state = state,
        .error = error,
    };
    if (!event_is_valid(&event)) {
        return false;
    }

    log->events[log->next_index] = event;
    log->next_index = (uint8_t)((log->next_index + 1U) % DESK_DIAGNOSTIC_EVENT_COUNT);
    if (log->count < DESK_DIAGNOSTIC_EVENT_COUNT) {
        ++log->count;
    }
    ++log->next_sequence;
    if (log->next_sequence == 0U) {
        log->next_sequence = 1U;
    }
    return true;
}

bool desk_diagnostic_log_get_newest(const desk_diagnostic_log_t *log, size_t offset,
                                    desk_diagnostic_event_t *event)
{
    if (log == NULL || event == NULL || offset >= log->count ||
        log->count > DESK_DIAGNOSTIC_EVENT_COUNT ||
        log->next_index >= DESK_DIAGNOSTIC_EVENT_COUNT) {
        return false;
    }
    size_t index =
        (log->next_index + DESK_DIAGNOSTIC_EVENT_COUNT - 1U - offset) % DESK_DIAGNOSTIC_EVENT_COUNT;
    *event = log->events[index];
    return event_is_valid(event);
}

bool desk_diagnostic_log_encode(const desk_diagnostic_log_t *log,
                                uint8_t encoded[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE])
{
    if (log == NULL || encoded == NULL || log->count > DESK_DIAGNOSTIC_EVENT_COUNT ||
        log->next_index >= DESK_DIAGNOSTIC_EVENT_COUNT || log->next_sequence == 0U ||
        (log->count < DESK_DIAGNOSTIC_EVENT_COUNT && log->next_index != log->count)) {
        return false;
    }

    uint8_t record[DESK_DIAGNOSTIC_LOG_ENCODED_SIZE] = {
        [0] = DESK_DIAGNOSTIC_LOG_VERSION,
        [1] = log->count,
        [2] = log->next_index,
    };
    write_u32_le(&record[4], log->next_sequence);
    for (size_t index = 0U; index < DESK_DIAGNOSTIC_EVENT_COUNT; ++index) {
        const desk_diagnostic_event_t *event = &log->events[index];
        bool used = log->count == DESK_DIAGNOSTIC_EVENT_COUNT || index < log->count;
        if (used && !event_is_valid(event)) {
            return false;
        }
        if (!used) {
            continue;
        }
        size_t offset =
            DESK_DIAGNOSTIC_LOG_HEADER_SIZE + (index * DESK_DIAGNOSTIC_EVENT_ENCODED_SIZE);
        write_u32_le(&record[offset], event->sequence);
        write_u32_le(&record[offset + 4U], event->reset_reason);
        write_u32_le(&record[offset + 8U], event->uptime_seconds);
        write_u32_le(&record[offset + 12U], (uint32_t)event->state);
        write_u32_le(&record[offset + 16U], (uint32_t)event->error);
    }
    write_u32_le(&record[DESK_DIAGNOSTIC_LOG_CRC_OFFSET],
                 desk_crc32_compute(record, DESK_DIAGNOSTIC_LOG_CRC_OFFSET));
    memcpy(encoded, record, sizeof(record));
    return true;
}

bool desk_diagnostic_log_decode(const uint8_t *encoded, size_t encoded_size,
                                desk_diagnostic_log_t *log)
{
    if (encoded == NULL || log == NULL || encoded_size != DESK_DIAGNOSTIC_LOG_ENCODED_SIZE ||
        encoded[0] != DESK_DIAGNOSTIC_LOG_VERSION || encoded[1] > DESK_DIAGNOSTIC_EVENT_COUNT ||
        encoded[2] >= DESK_DIAGNOSTIC_EVENT_COUNT || encoded[3] != 0U ||
        read_u32_le(&encoded[4]) == 0U ||
        (encoded[1] < DESK_DIAGNOSTIC_EVENT_COUNT && encoded[2] != encoded[1]) ||
        read_u32_le(&encoded[DESK_DIAGNOSTIC_LOG_CRC_OFFSET]) !=
            desk_crc32_compute(encoded, DESK_DIAGNOSTIC_LOG_CRC_OFFSET)) {
        return false;
    }

    desk_diagnostic_log_t decoded = {
        .next_sequence = read_u32_le(&encoded[4]),
        .count = encoded[1],
        .next_index = encoded[2],
    };
    for (size_t index = 0U; index < DESK_DIAGNOSTIC_EVENT_COUNT; ++index) {
        size_t offset =
            DESK_DIAGNOSTIC_LOG_HEADER_SIZE + (index * DESK_DIAGNOSTIC_EVENT_ENCODED_SIZE);
        desk_diagnostic_event_t event = {
            .sequence = read_u32_le(&encoded[offset]),
            .reset_reason = read_u32_le(&encoded[offset + 4U]),
            .uptime_seconds = read_u32_le(&encoded[offset + 8U]),
            .state = (desk_state_t)read_u32_le(&encoded[offset + 12U]),
            .error = (desk_error_t)read_u32_le(&encoded[offset + 16U]),
        };
        bool used = decoded.count == DESK_DIAGNOSTIC_EVENT_COUNT || index < decoded.count;
        if ((used && !event_is_valid(&event)) ||
            (!used &&
             (event.sequence != 0U || event.reset_reason != 0U || event.uptime_seconds != 0U ||
              event.state != DESK_STATE_SAFE_START || event.error != DESK_ERROR_NONE))) {
            return false;
        }
        decoded.events[index] = event;
    }
    *log = decoded;
    return true;
}
