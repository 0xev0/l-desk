#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t esp_ota_guard_confirm_boot_ready(uint32_t timeout_ms, bool network_service_ready);
