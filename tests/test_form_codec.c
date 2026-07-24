#include "desk_form_codec.h"
#include "test_support.h"

#include <string.h>

static void components_decode_percent_hex_and_spaces(void)
{
    char decoded[32];
    size_t length = 0U;
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_OK,
        desk_form_decode_component("Desk+2%2E4%20GHz", strlen("Desk+2%2E4%20GHz"), decoded,
                                   sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(12, length);
    TEST_ASSERT_TRUE(strcmp(decoded, "Desk 2.4 GHz") == 0);
}

static void malformed_components_and_overflow_are_rejected(void)
{
    char decoded[4];
    size_t length = 99U;
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_INVALID_ARGUMENT,
        desk_form_decode_component(NULL, 0U, decoded, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_INVALID_ARGUMENT,
        desk_form_decode_component("", 0U, NULL, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_INVALID_ARGUMENT,
        desk_form_decode_component("", 0U, decoded, 0U, &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_INVALID_ARGUMENT,
        desk_form_decode_component("", 0U, decoded, sizeof(decoded), NULL));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_MALFORMED,
        desk_form_decode_component("%", 1U, decoded, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_MALFORMED,
        desk_form_decode_component("%0", 2U, decoded, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_MALFORMED,
        desk_form_decode_component("%GG", 3U, decoded, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_MALFORMED,
        desk_form_decode_component("%00", 3U, decoded, sizeof(decoded), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_TOO_LONG,
        desk_form_decode_component("abcd", 4U, decoded, sizeof(decoded), &length));
}

static void named_values_are_bounded_and_unique(void)
{
    const char *form = "ssid=Workshop+WiFi&password=p%40ss%26word&mode=hybrid";
    char value[32];
    size_t length = 0U;
    TEST_ASSERT_EQUAL_INT(DESK_FORM_OK,
                          desk_form_get_value(form, strlen(form), "ssid", value,
                                              sizeof(value), &length));
    TEST_ASSERT_TRUE(strcmp(value, "Workshop WiFi") == 0);
    TEST_ASSERT_EQUAL_INT(13, length);
    TEST_ASSERT_EQUAL_INT(DESK_FORM_OK,
                          desk_form_get_value(form, strlen(form), "password", value,
                                              sizeof(value), &length));
    TEST_ASSERT_TRUE(strcmp(value, "p@ss&word") == 0);

    TEST_ASSERT_EQUAL_INT(DESK_FORM_NOT_FOUND,
                          desk_form_get_value(form, strlen(form), "token", value,
                                              sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_TOO_LONG,
                          desk_form_get_value(form, strlen(form), "ssid", value, 4U,
                                              &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_DUPLICATE_KEY,
        desk_form_get_value("ssid=a&ssid=b", 13U, "ssid", value, sizeof(value), &length));
}

static void malformed_forms_and_arguments_are_rejected(void)
{
    char value[8];
    size_t length;
    TEST_ASSERT_EQUAL_INT(DESK_FORM_INVALID_ARGUMENT,
                          desk_form_get_value(NULL, 0U, "a", value, sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_INVALID_ARGUMENT,
                          desk_form_get_value("a=b", 3U, "", value, sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_INVALID_ARGUMENT,
                          desk_form_get_value("a=b", 3U, "a", NULL, sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_INVALID_ARGUMENT,
                          desk_form_get_value("a=b", 3U, "a", value, 0U, &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_INVALID_ARGUMENT,
                          desk_form_get_value("a=b", 3U, "a", value, sizeof(value), NULL));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_MALFORMED,
                          desk_form_get_value("broken", 6U, "a", value, sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(DESK_FORM_MALFORMED,
                          desk_form_get_value("=value", 6U, "a", value, sizeof(value), &length));
    TEST_ASSERT_EQUAL_INT(
        DESK_FORM_MALFORMED,
        desk_form_get_value("%GG=value", 9U, "a", value, sizeof(value), &length));
}

int main(void)
{
    components_decode_percent_hex_and_spaces();
    malformed_components_and_overflow_are_rejected();
    named_values_are_bounded_and_unique();
    malformed_forms_and_arguments_are_rejected();
    return 0;
}
