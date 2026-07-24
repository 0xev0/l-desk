#include "desk_http_security.h"

#include <string.h>

static bool is_optional_space(char character)
{
    return character == ' ' || character == '\t';
}

static bool has_control_character(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)text[index];
        if (character < 0x20U || character == 0x7FU) {
            return true;
        }
    }
    return false;
}

desk_http_cookie_result_t desk_http_extract_cookie(const char *header, size_t header_length,
                                                   const char *name, char *value,
                                                   size_t value_capacity)
{
    if (header == NULL || name == NULL || name[0] == '\0' || value == NULL ||
        value_capacity == 0U || has_control_character(header, header_length)) {
        return DESK_HTTP_COOKIE_INVALID_ARGUMENT;
    }

    const size_t name_length = strlen(name);
    bool found = false;
    size_t segment_start = 0U;
    while (segment_start < header_length) {
        while (segment_start < header_length && is_optional_space(header[segment_start])) {
            ++segment_start;
        }
        size_t segment_end = segment_start;
        while (segment_end < header_length && header[segment_end] != ';') {
            ++segment_end;
        }
        size_t trimmed_end = segment_end;
        while (trimmed_end > segment_start && is_optional_space(header[trimmed_end - 1U])) {
            --trimmed_end;
        }

        size_t equals = segment_start;
        while (equals < trimmed_end && header[equals] != '=') {
            ++equals;
        }
        if (equals == segment_start || equals == trimmed_end) {
            return DESK_HTTP_COOKIE_MALFORMED;
        }

        size_t key_end = equals;
        while (key_end > segment_start && is_optional_space(header[key_end - 1U])) {
            --key_end;
        }
        size_t value_start = equals + 1U;
        while (value_start < trimmed_end && is_optional_space(header[value_start])) {
            ++value_start;
        }
        size_t cookie_value_length = trimmed_end - value_start;
        bool matches = key_end - segment_start == name_length &&
                       memcmp(&header[segment_start], name, name_length) == 0;
        if (matches) {
            if (found) {
                return DESK_HTTP_COOKIE_DUPLICATE;
            }
            if (cookie_value_length == 0U) {
                return DESK_HTTP_COOKIE_MALFORMED;
            }
            if (cookie_value_length + 1U > value_capacity) {
                return DESK_HTTP_COOKIE_TOO_LONG;
            }
            memcpy(value, &header[value_start], cookie_value_length);
            value[cookie_value_length] = '\0';
            found = true;
        }
        segment_start = segment_end + 1U;
    }
    return found ? DESK_HTTP_COOKIE_OK : DESK_HTTP_COOKIE_NOT_FOUND;
}

bool desk_http_origin_matches_host(const char *origin, const char *host)
{
    static const char prefix[] = "http://";
    if (origin == NULL || host == NULL || host[0] == '\0') {
        return false;
    }
    const size_t prefix_length = sizeof(prefix) - 1U;
    const size_t origin_length = strlen(origin);
    const size_t host_length = strlen(host);
    return origin_length == prefix_length + host_length &&
           memcmp(origin, prefix, prefix_length) == 0 &&
           memcmp(&origin[prefix_length], host, host_length) == 0;
}
