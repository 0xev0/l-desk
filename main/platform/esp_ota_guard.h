#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t esp_ota_guard_confirm_control_ready(uint32_t timeout_ms);
