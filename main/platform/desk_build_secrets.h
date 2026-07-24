#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DESK_BUILD_ADMIN_SECRET_SIZE 16U

typedef struct {
    const uint8_t *tls_certificate;
    size_t tls_certificate_size;
    const uint8_t *tls_private_key;
    size_t tls_private_key_size;
} desk_build_secrets_t;

bool desk_build_secrets_get(desk_build_secrets_t *secrets);
bool desk_build_secrets_copy_admin_secret(
    uint8_t output[DESK_BUILD_ADMIN_SECRET_SIZE]);
