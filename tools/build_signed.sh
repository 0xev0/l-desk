#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 /absolute/path/to/signing-key.pem" >&2
    exit 2
fi

key_path="$1"
if [[ ! -f "$key_path" ]]; then
    echo "signing key not found: $key_path" >&2
    exit 2
fi

idf_activate="${IDF_ACTIVATE:-/Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh}"
if [[ ! -f "$idf_activate" ]]; then
    echo "ESP-IDF activation script not found: $idf_activate" >&2
    exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# shellcheck disable=SC1090
set +u
source "$idf_activate" >/dev/null
set -u

python "$IDF_PATH/tools/idf.py" build
python -m espsecure sign-data \
    --version 2 \
    --keyfile "$key_path" \
    build/motorized_desk.bin
python -m espsecure verify-signature \
    --version 2 \
    --keyfile "$key_path" \
    build/motorized_desk.bin

echo "signed firmware: $repo_root/build/motorized_desk.bin"
