#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DESK_HTTP_COOKIE_OK = 0,
    DESK_HTTP_COOKIE_NOT_FOUND,
    DESK_HTTP_COOKIE_INVALID_ARGUMENT,
    DESK_HTTP_COOKIE_MALFORMED,
    DESK_HTTP_COOKIE_TOO_LONG,
    DESK_HTTP_COOKIE_DUPLICATE,
} desk_http_cookie_result_t;

desk_http_cookie_result_t desk_http_extract_cookie(const char *header, size_t header_length,
                                                   const char *name, char *value,
                                                   size_t value_capacity);
bool desk_http_origin_matches_host(const char *origin, const char *host);
