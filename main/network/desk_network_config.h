#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "desk_command_arbiter.h"

#define DESK_WIFI_SSID_MAX_LENGTH 32U
#define DESK_WIFI_PASSWORD_MIN_LENGTH 8U
#define DESK_WIFI_PASSWORD_MAX_LENGTH 64U

bool desk_network_credentials_are_valid(const char *ssid, size_t ssid_length, const char *password,
                                        size_t password_length);
bool desk_network_control_mode_is_valid(desk_control_mode_t mode);
