#include "desk_build_secrets.h"

#include <string.h>

#include "generated/desk_build_secrets.inc"

_Static_assert(sizeof(generated_admin_secret) == DESK_BUILD_ADMIN_SECRET_SIZE,
               "generated administration secret must be 128 bits");

bool desk_build_secrets_get(desk_build_secrets_t *secrets)
{
    if (secrets == NULL || sizeof(generated_tls_certificate) <= 1U ||
        sizeof(generated_tls_private_key) <= 1U) {
        return false;
    }
    *secrets = (desk_build_secrets_t){
        .tls_certificate = generated_tls_certificate,
        .tls_certificate_size = sizeof(generated_tls_certificate),
        .tls_private_key = generated_tls_private_key,
        .tls_private_key_size = sizeof(generated_tls_private_key),
    };
    return true;
}

bool desk_build_secrets_copy_admin_secret(
    uint8_t output[DESK_BUILD_ADMIN_SECRET_SIZE])
{
    if (output == NULL) {
        return false;
    }
    memcpy(output, generated_admin_secret, sizeof(generated_admin_secret));
    return true;
}
