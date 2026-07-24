#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DESK_AUTH_SECRET_BYTES 16U
#define DESK_AUTH_SECRET_HEX_LENGTH (DESK_AUTH_SECRET_BYTES * 2U)
#define DESK_AUTH_SECRET_TEXT_SIZE (DESK_AUTH_SECRET_HEX_LENGTH + 1U)
#define DESK_AUTH_SESSION_COUNT 4U
#define DESK_AUTH_MAX_FAILURES 5U
#define DESK_AUTH_BLOCK_MS 60000U
#define DESK_AUTH_SESSION_LIFETIME_MS (8U * 60U * 60U * 1000U)

typedef enum {
    DESK_AUTH_LOGIN_OK = 0,
    DESK_AUTH_LOGIN_INVALID_ARGUMENT,
    DESK_AUTH_LOGIN_DENIED,
    DESK_AUTH_LOGIN_RATE_LIMITED,
    DESK_AUTH_LOGIN_INVALID_RANDOM,
    DESK_AUTH_LOGIN_SESSION_CAPACITY,
} desk_auth_login_result_t;

typedef struct {
    uint8_t secret[DESK_AUTH_SECRET_BYTES];
    uint64_t expires_at_ms;
    bool active;
} desk_auth_session_t;

typedef struct {
    uint8_t admin_secret[DESK_AUTH_SECRET_BYTES];
    desk_auth_session_t sessions[DESK_AUTH_SESSION_COUNT];
    uint64_t blocked_until_ms;
    uint8_t failed_attempts;
    bool initialized;
} desk_authenticator_t;

bool desk_auth_format_secret(const uint8_t secret[DESK_AUTH_SECRET_BYTES],
                             char output[DESK_AUTH_SECRET_TEXT_SIZE]);
void desk_authenticator_init(desk_authenticator_t *authenticator,
                             const uint8_t admin_secret[DESK_AUTH_SECRET_BYTES]);
desk_auth_login_result_t
desk_authenticator_login(desk_authenticator_t *authenticator, const char *admin_secret_hex,
                         const uint8_t session_random[DESK_AUTH_SECRET_BYTES], uint64_t now_ms,
                         char session_hex[DESK_AUTH_SECRET_TEXT_SIZE]);
bool desk_authenticator_validate_session(desk_authenticator_t *authenticator,
                                         const char *session_hex, uint64_t now_ms);
bool desk_authenticator_logout(desk_authenticator_t *authenticator, const char *session_hex);
