#pragma once

#include <stddef.h>

typedef enum {
    DESK_FORM_OK = 0,
    DESK_FORM_NOT_FOUND,
    DESK_FORM_INVALID_ARGUMENT,
    DESK_FORM_MALFORMED,
    DESK_FORM_TOO_LONG,
    DESK_FORM_DUPLICATE_KEY,
} desk_form_result_t;

desk_form_result_t desk_form_decode_component(const char *encoded, size_t encoded_length,
                                              char *decoded, size_t decoded_capacity,
                                              size_t *decoded_length);
desk_form_result_t desk_form_get_value(const char *form, size_t form_length, const char *key,
                                       char *value, size_t value_capacity,
                                       size_t *value_length);
