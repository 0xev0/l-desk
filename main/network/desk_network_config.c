#include "desk_network_config.h"

#include <stddef.h>

static bool has_forbidden_control_character(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)text[index];
        if (character < 0x20U || character == 0x7FU) {
            return true;
        }
    }
    return false;
}

bool desk_network_credentials_are_valid(const char *ssid, size_t ssid_length,
                                        const char *password, size_t password_length)
{
    return ssid != NULL && password != NULL && ssid_length > 0U &&
           ssid_length <= DESK_WIFI_SSID_MAX_LENGTH &&
           password_length >= DESK_WIFI_PASSWORD_MIN_LENGTH &&
           password_length <= DESK_WIFI_PASSWORD_MAX_LENGTH &&
           !has_forbidden_control_character(ssid, ssid_length) &&
           !has_forbidden_control_character(password, password_length);
}

bool desk_network_control_mode_is_valid(desk_control_mode_t mode)
{
    return mode == DESK_CONTROL_MODE_LOCAL_ONLY || mode == DESK_CONTROL_MODE_HYBRID ||
           mode == DESK_CONTROL_MODE_MOBILE_ONLY;
}
