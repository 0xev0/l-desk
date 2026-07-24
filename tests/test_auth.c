#include "desk_auth.h"
#include "test_support.h"

static void fill_secret(uint8_t secret[DESK_AUTH_SECRET_BYTES], uint8_t start)
{
    for (size_t index = 0U; index < DESK_AUTH_SECRET_BYTES; ++index) {
        secret[index] = (uint8_t)(start + index);
    }
}

static void successful_login_creates_and_validates_bounded_session(void)
{
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    uint8_t random[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 1U);
    fill_secret(random, 33U);
    char admin_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, admin_hex));

    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, admin);
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_OK,
        desk_authenticator_login(&authenticator, admin_hex, random, 1000U, session_hex));
    TEST_ASSERT_TRUE(desk_authenticator_validate_session(&authenticator, session_hex, 1001U));
    TEST_ASSERT_TRUE(desk_authenticator_validate_session(
        &authenticator, session_hex, 1000U + DESK_AUTH_SESSION_LIFETIME_MS - 1U));
    TEST_ASSERT_FALSE(desk_authenticator_validate_session(
        &authenticator, session_hex, 1000U + DESK_AUTH_SESSION_LIFETIME_MS));
}

static void mixed_case_hex_is_accepted_but_malformed_secrets_are_denied(void)
{
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    uint8_t random[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 0xA0U);
    fill_secret(random, 0x10U);
    char admin_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, admin_hex));
    for (size_t index = 0U; index < DESK_AUTH_SECRET_HEX_LENGTH; ++index) {
        if (admin_hex[index] >= 'a' && admin_hex[index] <= 'f') {
            admin_hex[index] = (char)(admin_hex[index] - 'a' + 'A');
        }
    }

    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, admin);
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_OK,
        desk_authenticator_login(&authenticator, admin_hex, random, 0U, session_hex));

    admin_hex[0] = 'x';
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_DENIED,
        desk_authenticator_login(&authenticator, admin_hex, random, 1U, session_hex));
    admin_hex[0] = '\0';
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_DENIED,
        desk_authenticator_login(&authenticator, admin_hex, random, 2U, session_hex));
}

static void repeated_failures_are_rate_limited_then_recover(void)
{
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    uint8_t random[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 1U);
    fill_secret(random, 50U);
    char valid_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, valid_hex));
    char invalid_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    for (size_t index = 0U; index < DESK_AUTH_SECRET_HEX_LENGTH; ++index) {
        invalid_hex[index] = '0';
    }
    invalid_hex[DESK_AUTH_SECRET_HEX_LENGTH] = '\0';

    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, admin);
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    for (uint8_t attempt = 0U; attempt < DESK_AUTH_MAX_FAILURES; ++attempt) {
        TEST_ASSERT_EQUAL_INT(
            DESK_AUTH_LOGIN_DENIED,
            desk_authenticator_login(&authenticator, invalid_hex, random, 100U, session_hex));
    }
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_RATE_LIMITED,
        desk_authenticator_login(&authenticator, valid_hex, random,
                                 100U + DESK_AUTH_BLOCK_MS - 1U, session_hex));
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_OK,
        desk_authenticator_login(&authenticator, valid_hex, random, 100U + DESK_AUTH_BLOCK_MS,
                                 session_hex));
}

static void sessions_have_fixed_capacity_and_logout_is_explicit(void)
{
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 1U);
    char admin_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, admin_hex));
    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, admin);

    char sessions[DESK_AUTH_SESSION_COUNT][DESK_AUTH_SECRET_TEXT_SIZE];
    for (size_t session = 0U; session < DESK_AUTH_SESSION_COUNT; ++session) {
        uint8_t random[DESK_AUTH_SECRET_BYTES];
        fill_secret(random, (uint8_t)(20U + session * DESK_AUTH_SECRET_BYTES));
        TEST_ASSERT_EQUAL_INT(
            DESK_AUTH_LOGIN_OK,
            desk_authenticator_login(&authenticator, admin_hex, random, 0U, sessions[session]));
    }
    uint8_t extra[DESK_AUTH_SECRET_BYTES];
    fill_secret(extra, 100U);
    char ignored[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_SESSION_CAPACITY,
        desk_authenticator_login(&authenticator, admin_hex, extra, 1U, ignored));

    TEST_ASSERT_TRUE(desk_authenticator_logout(&authenticator, sessions[1]));
    TEST_ASSERT_FALSE(desk_authenticator_validate_session(&authenticator, sessions[1], 2U));
    TEST_ASSERT_FALSE(desk_authenticator_logout(&authenticator, sessions[1]));
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_OK,
        desk_authenticator_login(&authenticator, admin_hex, extra, 2U, ignored));
}

static void invalid_initialization_random_and_arguments_fail_closed(void)
{
    uint8_t zero[DESK_AUTH_SECRET_BYTES] = {0};
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 1U);
    char admin_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, admin_hex));
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE];

    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, zero);
    TEST_ASSERT_FALSE(authenticator.initialized);
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_INVALID_ARGUMENT,
        desk_authenticator_login(&authenticator, admin_hex, zero, 0U, session_hex));

    desk_authenticator_init(&authenticator, admin);
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_INVALID_RANDOM,
        desk_authenticator_login(&authenticator, admin_hex, zero, 0U, session_hex));
    TEST_ASSERT_FALSE(desk_auth_format_secret(NULL, session_hex));
    TEST_ASSERT_FALSE(desk_auth_format_secret(admin, NULL));
    desk_authenticator_init(NULL, admin);
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_INVALID_ARGUMENT,
        desk_authenticator_login(NULL, NULL, NULL, 0U, NULL));
    TEST_ASSERT_FALSE(desk_authenticator_validate_session(NULL, NULL, 0U));
    TEST_ASSERT_FALSE(desk_authenticator_logout(NULL, NULL));
}

static void duplicate_session_random_is_rejected(void)
{
    uint8_t admin[DESK_AUTH_SECRET_BYTES];
    uint8_t random[DESK_AUTH_SECRET_BYTES];
    fill_secret(admin, 1U);
    fill_secret(random, 44U);
    char admin_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_TRUE(desk_auth_format_secret(admin, admin_hex));
    desk_authenticator_t authenticator;
    desk_authenticator_init(&authenticator, admin);
    char session_hex[DESK_AUTH_SECRET_TEXT_SIZE];
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_OK,
        desk_authenticator_login(&authenticator, admin_hex, random, 0U, session_hex));
    TEST_ASSERT_EQUAL_INT(
        DESK_AUTH_LOGIN_INVALID_RANDOM,
        desk_authenticator_login(&authenticator, admin_hex, random, 1U, session_hex));
}

int main(void)
{
    successful_login_creates_and_validates_bounded_session();
    mixed_case_hex_is_accepted_but_malformed_secrets_are_denied();
    repeated_failures_are_rate_limited_then_recover();
    sessions_have_fixed_capacity_and_logout_is_explicit();
    invalid_initialization_random_and_arguments_fail_closed();
    duplicate_session_random_is_rejected();
    return 0;
}
