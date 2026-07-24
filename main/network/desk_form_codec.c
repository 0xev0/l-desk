#include "desk_form_codec.h"

#include <stdbool.h>
#include <string.h>

#define DESK_FORM_KEY_BUFFER_SIZE 32U

static bool hex_nibble(char character, unsigned char *nibble)
{
    if (character >= '0' && character <= '9') {
        *nibble = (unsigned char)(character - '0');
        return true;
    }
    if (character >= 'a' && character <= 'f') {
        *nibble = (unsigned char)(character - 'a' + 10);
        return true;
    }
    if (character >= 'A' && character <= 'F') {
        *nibble = (unsigned char)(character - 'A' + 10);
        return true;
    }
    return false;
}

desk_form_result_t desk_form_decode_component(const char *encoded, size_t encoded_length,
                                              char *decoded, size_t decoded_capacity,
                                              size_t *decoded_length)
{
    if (encoded == NULL || decoded == NULL || decoded_capacity == 0U || decoded_length == NULL) {
        return DESK_FORM_INVALID_ARGUMENT;
    }

    size_t output_index = 0U;
    for (size_t input_index = 0U; input_index < encoded_length; ++input_index) {
        unsigned char character = (unsigned char)encoded[input_index];
        if (character == '%') {
            if (encoded_length - input_index < 3U) {
                return DESK_FORM_MALFORMED;
            }
            unsigned char high;
            unsigned char low;
            if (!hex_nibble(encoded[input_index + 1U], &high) ||
                !hex_nibble(encoded[input_index + 2U], &low)) {
                return DESK_FORM_MALFORMED;
            }
            character = (unsigned char)((high << 4U) | low);
            input_index += 2U;
        } else if (character == '+') {
            character = ' ';
        }

        if (character == '\0') {
            return DESK_FORM_MALFORMED;
        }
        if (output_index + 1U >= decoded_capacity) {
            return DESK_FORM_TOO_LONG;
        }
        decoded[output_index] = (char)character;
        ++output_index;
    }

    decoded[output_index] = '\0';
    *decoded_length = output_index;
    return DESK_FORM_OK;
}

static desk_form_result_t decode_and_compare_key(const char *encoded, size_t encoded_length,
                                                 const char *expected, bool *matches)
{
    char decoded[DESK_FORM_KEY_BUFFER_SIZE];
    size_t decoded_length;
    desk_form_result_t result = desk_form_decode_component(encoded, encoded_length, decoded,
                                                           sizeof(decoded), &decoded_length);
    if (result != DESK_FORM_OK) {
        return result;
    }
    size_t expected_length = strlen(expected);
    *matches = decoded_length == expected_length && memcmp(decoded, expected, expected_length) == 0;
    return DESK_FORM_OK;
}

desk_form_result_t desk_form_get_value(const char *form, size_t form_length, const char *key,
                                       char *value, size_t value_capacity, size_t *value_length)
{
    if (form == NULL || key == NULL || key[0] == '\0' || value == NULL || value_capacity == 0U ||
        value_length == NULL) {
        return DESK_FORM_INVALID_ARGUMENT;
    }

    bool found = false;
    size_t field_start = 0U;
    while (field_start < form_length) {
        size_t field_end = field_start;
        while (field_end < form_length && form[field_end] != '&') {
            ++field_end;
        }

        size_t equals = field_start;
        while (equals < field_end && form[equals] != '=') {
            ++equals;
        }
        if (equals == field_start || equals == field_end) {
            return DESK_FORM_MALFORMED;
        }

        bool matches;
        desk_form_result_t result =
            decode_and_compare_key(&form[field_start], equals - field_start, key, &matches);
        if (result != DESK_FORM_OK) {
            return result;
        }
        if (matches) {
            if (found) {
                return DESK_FORM_DUPLICATE_KEY;
            }
            result = desk_form_decode_component(&form[equals + 1U], field_end - equals - 1U, value,
                                                value_capacity, value_length);
            if (result != DESK_FORM_OK) {
                return result;
            }
            found = true;
        }

        field_start = field_end + 1U;
    }
    return found ? DESK_FORM_OK : DESK_FORM_NOT_FOUND;
}
