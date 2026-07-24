#include "desk_network_config.h"
#include "test_support.h"

static void valid_credentials_cover_boundary_lengths(void)
{
    char ssid[DESK_WIFI_SSID_MAX_LENGTH + 1U];
    char password[DESK_WIFI_PASSWORD_MAX_LENGTH + 1U];
    for (size_t index = 0U; index < DESK_WIFI_SSID_MAX_LENGTH; ++index) {
        ssid[index] = 's';
    }
    ssid[DESK_WIFI_SSID_MAX_LENGTH] = '\0';
    for (size_t index = 0U; index < DESK_WIFI_PASSWORD_MAX_LENGTH; ++index) {
        password[index] = 'p';
    }
    password[DESK_WIFI_PASSWORD_MAX_LENGTH] = '\0';

    TEST_ASSERT_TRUE(desk_network_credentials_are_valid(ssid, DESK_WIFI_SSID_MAX_LENGTH, password,
                                                        DESK_WIFI_PASSWORD_MAX_LENGTH));
    TEST_ASSERT_TRUE(
        desk_network_credentials_are_valid(ssid, 1U, password, DESK_WIFI_PASSWORD_MIN_LENGTH));
}

static void invalid_lengths_pointers_and_controls_are_rejected(void)
{
    const char *ssid = "desk";
    const char *password = "password";
    TEST_ASSERT_FALSE(desk_network_credentials_are_valid(NULL, 4U, password, 8U));
    TEST_ASSERT_FALSE(desk_network_credentials_are_valid(ssid, 4U, NULL, 8U));
    TEST_ASSERT_FALSE(desk_network_credentials_are_valid(ssid, 0U, password, 8U));
    TEST_ASSERT_FALSE(
        desk_network_credentials_are_valid(ssid, DESK_WIFI_SSID_MAX_LENGTH + 1U, password, 8U));
    TEST_ASSERT_FALSE(
        desk_network_credentials_are_valid(ssid, 4U, password, DESK_WIFI_PASSWORD_MIN_LENGTH - 1U));
    TEST_ASSERT_FALSE(
        desk_network_credentials_are_valid(ssid, 4U, password, DESK_WIFI_PASSWORD_MAX_LENGTH + 1U));

    const char bad_ssid[] = {'d', '\n', 's', 'k'};
    TEST_ASSERT_FALSE(desk_network_credentials_are_valid(bad_ssid, sizeof(bad_ssid), password, 8U));
    const char bad_password[] = {'p', 'a', 's', 's', '\0', 'o', 'r', 'd'};
    TEST_ASSERT_FALSE(
        desk_network_credentials_are_valid(ssid, 4U, bad_password, sizeof(bad_password)));
}

static void every_control_mode_is_explicit(void)
{
    TEST_ASSERT_TRUE(desk_network_control_mode_is_valid(DESK_CONTROL_MODE_LOCAL_ONLY));
    TEST_ASSERT_TRUE(desk_network_control_mode_is_valid(DESK_CONTROL_MODE_HYBRID));
    TEST_ASSERT_TRUE(desk_network_control_mode_is_valid(DESK_CONTROL_MODE_MOBILE_ONLY));
    TEST_ASSERT_FALSE(desk_network_control_mode_is_valid((desk_control_mode_t)99));
}

int main(void)
{
    valid_credentials_cover_boundary_lengths();
    invalid_lengths_pointers_and_controls_are_rejected();
    every_control_mode_is_explicit();
    return 0;
}
