#!/usr/bin/env bash

set -euo pipefail

usage() {
    cat >&2 <<'EOF'
Usage:
  tools/build_release.sh \
    --signing-key /secure/desk-signing.pem \
    --tls-certificate /secure/fullchain.pem \
    --tls-private-key /secure/privkey.pem \
    --credentials-output /secure/device-credentials.txt \
    [--admin-token 32_HEX_CHARACTERS]
EOF
    exit 2
}

signing_key=""
tls_certificate=""
tls_private_key=""
credentials_output=""
admin_token=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --signing-key)
        [[ $# -ge 2 ]] || usage
        signing_key="$2"
        shift 2
        ;;
    --tls-certificate)
        [[ $# -ge 2 ]] || usage
        tls_certificate="$2"
        shift 2
        ;;
    --tls-private-key)
        [[ $# -ge 2 ]] || usage
        tls_private_key="$2"
        shift 2
        ;;
    --credentials-output)
        [[ $# -ge 2 ]] || usage
        credentials_output="$2"
        shift 2
        ;;
    --admin-token)
        [[ $# -ge 2 ]] || usage
        admin_token="$2"
        shift 2
        ;;
    *)
        usage
        ;;
    esac
done

[[ -n "$signing_key" && -n "$tls_certificate" && -n "$tls_private_key" &&
    -n "$credentials_output" ]] || usage
[[ -f "$signing_key" ]] || {
    echo "signing key not found: $signing_key" >&2
    exit 2
}
[[ -f "$tls_certificate" ]] || {
    echo "TLS certificate not found: $tls_certificate" >&2
    exit 2
}
[[ -f "$tls_private_key" ]] || {
    echo "TLS private key not found: $tls_private_key" >&2
    exit 2
}

idf_activate="${IDF_ACTIVATE:-/Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh}"
[[ -f "$idf_activate" ]] || {
    echo "ESP-IDF activation script not found: $idf_activate" >&2
    exit 2
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
generated_dir="$repo_root/main/generated"
generated_secrets="$generated_dir/desk_build_secrets.inc"
release_dir="$repo_root/build-release"

cleanup() {
    rm -f "$generated_secrets"
    rmdir "$generated_dir" 2>/dev/null || true
}
trap cleanup EXIT

certificate_public_key="$(
    openssl x509 -in "$tls_certificate" -pubkey -noout |
        openssl pkey -pubin -outform DER |
        openssl dgst -sha256
)"
private_public_key="$(
    openssl pkey -in "$tls_private_key" -pubout -outform DER |
        openssl dgst -sha256
)"
[[ "$certificate_public_key" == "$private_public_key" ]] || {
    echo "TLS certificate and private key do not match" >&2
    exit 2
}
openssl x509 -in "$tls_certificate" -checkend 604800 -noout >/dev/null || {
    echo "TLS certificate expires in less than seven days" >&2
    exit 2
}
openssl rsa -in "$signing_key" -check -noout >/dev/null

generator_arguments=(
    --certificate "$tls_certificate"
    --private-key "$tls_private_key"
    --output "$generated_secrets"
    --credentials-output "$credentials_output"
)
if [[ -n "$admin_token" ]]; then
    generator_arguments+=(--admin-token "$admin_token")
fi
python3 "$repo_root/tools/generate_build_secrets.py" "${generator_arguments[@]}"

set -a
eval "$("$idf_activate" -e)"
PATH="$PATH:$SYSTEM_PATH"
set +a

cmake -E remove_directory "$release_dir"
python "$IDF_PATH/tools/idf.py" \
    -B "$release_dir" \
    -D "SDKCONFIG=$release_dir/sdkconfig" \
    -D "SDKCONFIG_DEFAULTS=$repo_root/sdkconfig.defaults;$repo_root/sdkconfig.release.defaults" \
    set-target esp32s3
python "$IDF_PATH/tools/idf.py" -B "$release_dir" build

python -m espsecure sign-data \
    --version 2 \
    --keyfile "$signing_key" \
    --output "$release_dir/motorized_desk-signed.bin" \
    "$release_dir/motorized_desk.bin"
python -m espsecure sign-data \
    --version 2 \
    --keyfile "$signing_key" \
    --output "$release_dir/bootloader/bootloader-signed.bin" \
    "$release_dir/bootloader/bootloader.bin"
python -m espsecure verify-signature \
    --version 2 \
    --keyfile "$signing_key" \
    "$release_dir/motorized_desk-signed.bin"
python -m espsecure verify-signature \
    --version 2 \
    --keyfile "$signing_key" \
    "$release_dir/bootloader/bootloader-signed.bin"

chmod 600 \
    "$release_dir/motorized_desk-signed.bin" \
    "$release_dir/bootloader/bootloader-signed.bin"

(
    cd "$release_dir"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum \
            motorized_desk-signed.bin \
            bootloader/bootloader-signed.bin \
            partition_table/partition-table.bin \
            ota_data_initial.bin >SHA256SUMS
    else
        shasum -a 256 \
            motorized_desk-signed.bin \
            bootloader/bootloader-signed.bin \
            partition_table/partition-table.bin \
            ota_data_initial.bin >SHA256SUMS
    fi
)

cat <<EOF
Secure release built and verified:
  application: $release_dir/motorized_desk-signed.bin
  bootloader:  $release_dir/bootloader/bootloader-signed.bin
  credentials: $credentials_output

Do not flash before completing docs/secure-deployment.md.
EOF
