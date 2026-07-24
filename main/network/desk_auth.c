#include "desk_auth.h"

#include <limits.h>
#include <stddef.h>

static uint64_t saturating_deadline(uint64_t now_ms, uint32_t duration_ms)
{
    return UINT64_MAX - now_ms < duration_ms ? UINT64_MAX : now_ms + duration_ms;
}

static bool secret_is_nonzero(const uint8_t secret[DESK_AUTH_SECRET_BYTES])
{
    uint8_t combined = 0U;
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        combined |= secret[index];
    }
    return combined != 0U;
}

static uint8_t decode_nibble(char character, bool *valid)
{
    if (character >= '0' && character <= '9') {
        return (uint8_t)(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return (uint8_t)(character - 'a' + 10);
    }
    if (character >= 'A' && character <= 'F') {
        return (uint8_t)(character - 'A' + 10);
    }
    *valid = false;
    return 0U;
}

static bool parse_secret(const char *text, uint8_t secret[DESK_AUTH_SECRET_BYTES])
{
    if (text == NULL || secret == NULL) {
        return false;
    }

    bool valid = true;
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        uint8_t high = decode_nibble(text[index * 2U], &valid);
        uint8_t low = decode_nibble(text[index * 2U + 1U], &valid);
        secret[index] = (uint8_t)((high << 4U) | low);
    }
    return valid && text[DESK_AUTH_SECRET_HEX_LENGTH] == '\0';
}

static bool secrets_equal(const uint8_t left[DESK_AUTH_SECRET_BYTES],
                          const uint8_t right[DESK_AUTH_SECRET_BYTES])
{
    uint8_t difference = 0U;
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        difference |= (uint8_t)(left[index] ^ right[index]);
    }
    return difference == 0U;
}

bool desk_auth_format_secret(const uint8_t secret[DESK_AUTH_SECRET_BYTES],
                             char output[DESK_AUTH_SECRET_TEXT_SIZE])
{
    static const char hex[] = "0123456789abcdef";
    if (secret == NULL || output == NULL) {
        return false;
    }
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        output[index * 2U] = hex[secret[index] >> 4U];
        output[index * 2U + 1U] = hex[secret[index] & 0x0FU];
    }
    output[DESK_AUTH_SECRET_HEX_LENGTH] = '\0';
    return true;
}

void desk_authenticator_init(
    desk_authenticator_t *authenticator,
    const uint8_t admin_secret[DESK_AUTH_SECRET_BYTES])
{
    if (authenticator == NULL) {
        return;
    }
    *authenticator = (desk_authenticator_t){0};
    if (admin_secret == NULL || !secret_is_nonzero(admin_secret)) {
        return;
    }
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        authenticator->admin_secret[index] = admin_secret[index];
    }
    authenticator->initialized = true;
}

static void record_failed_login(desk_authenticator_t *authenticator, uint64_t now_ms)
{
    if (authenticator->failed_attempts < UINT8_MAX) {
        ++authenticator->failed_attempts;
    }
    if (authenticator->failed_attempts >= DESK_AUTH_MAX_FAILURES) {
        authenticator->blocked_until_ms = saturating_deadline(now_ms, DESK_AUTH_BLOCK_MS);
        authenticator->failed_attempts = 0U;
    }
}

static void expire_sessions(desk_authenticator_t *authenticator, uint64_t now_ms)
{
    for (size_t index = 0U; index < DESK_AUTH_SESSION_COUNT; ++index) {
        desk_auth_session_t *session = &authenticator->sessions[index];
        if (session->active && now_ms >= session->expires_at_ms) {
            *session = (desk_auth_session_t){0};
        }
    }
}

static desk_auth_session_t *find_free_session(desk_authenticator_t *authenticator)
{
    for (size_t index = 0U; index < DESK_AUTH_SESSION_COUNT; ++index) {
        if (!authenticator->sessions[index].active) {
            return &authenticator->sessions[index];
        }
    }
    return NULL;
}

static bool session_secret_exists(const desk_authenticator_t *authenticator,
                                  const uint8_t secret[DESK_AUTH_SECRET_BYTES])
{
    bool exists = false;
    for (size_t index = 0U; index < DESK_AUTH_SESSION_COUNT; ++index) {
        const desk_auth_session_t *session = &authenticator->sessions[index];
        bool equal = secrets_equal(session->secret, secret);
        exists = exists || (session->active && equal);
    }
    return exists;
}

desk_auth_login_result_t desk_authenticator_login(
    desk_authenticator_t *authenticator, const char *admin_secret_hex,
    const uint8_t session_random[DESK_AUTH_SECRET_BYTES], uint64_t now_ms,
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE])
{
    if (authenticator == NULL || !authenticator->initialized || admin_secret_hex == NULL ||
        session_random == NULL || session_hex == NULL) {
        return DESK_AUTH_LOGIN_INVALID_ARGUMENT;
    }
    if (now_ms < authenticator->blocked_until_ms) {
        return DESK_AUTH_LOGIN_RATE_LIMITED;
    }
    authenticator->blocked_until_ms = 0U;

    uint8_t supplied_secret[DESK_AUTH_SECRET_BYTES];
    bool parsed = parse_secret(admin_secret_hex, supplied_secret);
    if (!parsed || !secrets_equal(authenticator->admin_secret, supplied_secret)) {
        record_failed_login(authenticator, now_ms);
        return DESK_AUTH_LOGIN_DENIED;
    }
    expire_sessions(authenticator, now_ms);
    if (!secret_is_nonzero(session_random) ||
        session_secret_exists(authenticator, session_random)) {
        return DESK_AUTH_LOGIN_INVALID_RANDOM;
    }

    desk_auth_session_t *session = find_free_session(authenticator);
    if (session == NULL) {
        return DESK_AUTH_LOGIN_SESSION_CAPACITY;
    }
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        session->secret[index] = session_random[index];
    }
    session->expires_at_ms = saturating_deadline(now_ms, DESK_AUTH_SESSION_LIFETIME_MS);
    session->active = true;
    authenticator->failed_attempts = 0U;
    (void)desk_auth_format_secret(session->secret, session_hex);
    return DESK_AUTH_LOGIN_OK;
}

bool desk_authenticator_validate_session(desk_authenticator_t *authenticator,
                                         const char *session_hex, uint64_t now_ms)
{
    if (authenticator == NULL || !authenticator->initialized || session_hex == NULL) {
        return false;
    }
    uint8_t supplied_secret[DESK_AUTH_SECRET_BYTES];
    if (!parse_secret(session_hex, supplied_secret)) {
        return false;
    }

    expire_sessions(authenticator, now_ms);
    bool valid = false;
    for (size_t index = 0U; index < DESK_AUTH_SESSION_COUNT; ++index) {
        const desk_auth_session_t *session = &authenticator->sessions[index];
        bool equal = secrets_equal(session->secret, supplied_secret);
        valid = valid || (session->active && equal);
    }
    return valid;
}

bool desk_authenticator_logout(desk_authenticator_t *authenticator,
                               const char *session_hex)
{
    if (authenticator == NULL || !authenticator->initialized || session_hex == NULL) {
        return false;
    }
    uint8_t supplied_secret[DESK_AUTH_SECRET_BYTES];
    if (!parse_secret(session_hex, supplied_secret)) {
        return false;
    }

    bool removed = false;
    for (size_t index = 0U; index < DESK_AUTH_SESSION_COUNT; ++index) {
        desk_auth_session_t *session = &authenticator->sessions[index];
        if (session->active && secrets_equal(session->secret, supplied_secret)) {
            *session = (desk_auth_session_t){0};
            removed = true;
        }
    }
    return removed;
}
