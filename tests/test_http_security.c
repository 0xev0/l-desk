#include "desk_http_security.h"
#include "test_support.h"

#include <string.h>

static void exact_cookie_is_extracted(void)
{
    const char *header = "theme=dark; desk_session=0123456789abcdef; mode=hybrid";
    char value[32];
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_OK,
        desk_http_extract_cookie(header, strlen(header), "desk_session", value,
                                 sizeof(value)));
    TEST_ASSERT_TRUE(strcmp(value, "0123456789abcdef") == 0);

    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_NOT_FOUND,
        desk_http_extract_cookie(header, strlen(header), "session", value, sizeof(value)));
}

static void malformed_duplicate_and_oversize_cookies_are_rejected(void)
{
    char value[8];
    TEST_ASSERT_EQUAL_INT(DESK_HTTP_COOKIE_INVALID_ARGUMENT,
                          desk_http_extract_cookie(NULL, 0U, "a", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_INVALID_ARGUMENT,
        desk_http_extract_cookie("a=b", 3U, "", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_INVALID_ARGUMENT,
        desk_http_extract_cookie("a=\nb", 4U, "a", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_MALFORMED,
        desk_http_extract_cookie("broken", 6U, "a", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_MALFORMED,
        desk_http_extract_cookie("a=", 2U, "a", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_DUPLICATE,
        desk_http_extract_cookie("a=one; a=two", 12U, "a", value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        DESK_HTTP_COOKIE_TOO_LONG,
        desk_http_extract_cookie("a=12345678", 10U, "a", value, sizeof(value)));
}

static void browser_origin_must_match_the_exact_host(void)
{
    TEST_ASSERT_TRUE(desk_http_origin_matches_host("http://192.168.4.1",
                                                   "192.168.4.1"));
    TEST_ASSERT_TRUE(desk_http_origin_matches_host("http://desk.local:8080",
                                                   "desk.local:8080"));
    TEST_ASSERT_FALSE(desk_http_origin_matches_host("https://desk.local", "desk.local"));
    TEST_ASSERT_FALSE(desk_http_origin_matches_host("http://desk.local.attacker",
                                                    "desk.local"));
    TEST_ASSERT_FALSE(desk_http_origin_matches_host(NULL, "desk.local"));
    TEST_ASSERT_FALSE(desk_http_origin_matches_host("http://desk.local", ""));
}

int main(void)
{
    exact_cookie_is_extracted();
    malformed_duplicate_and_oversize_cookies_are_rejected();
    browser_origin_must_match_the_exact_host();
    return 0;
}
