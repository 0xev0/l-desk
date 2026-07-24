#!/usr/bin/env python3

"""Generate per-device build secrets without writing credentials to Git."""

from __future__ import annotations

import argparse
import json
import os
import secrets
import tempfile
from pathlib import Path


ADMIN_SECRET_BYTES = 16
MAX_CERTIFICATE_BYTES = 16 * 1024
MAX_PRIVATE_KEY_BYTES = 8 * 1024


def read_pem(path: Path, begin_marker: str, maximum_size: int) -> str:
    payload = path.read_bytes()
    if not payload or len(payload) > maximum_size or b"\0" in payload:
        raise ValueError(f"invalid PEM size: {path}")
    text = payload.decode("ascii")
    if begin_marker not in text or "-----END " not in text:
        raise ValueError(f"invalid PEM envelope: {path}")
    return text if text.endswith("\n") else f"{text}\n"


def atomic_private_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.")
    temporary_path = Path(temporary_name)
    try:
        os.fchmod(descriptor, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        os.replace(temporary_path, path)
    finally:
        temporary_path.unlink(missing_ok=True)


def parse_admin_token(value: str | None) -> bytes:
    if value is None:
        return secrets.token_bytes(ADMIN_SECRET_BYTES)
    try:
        token = bytes.fromhex(value)
    except ValueError as error:
        raise ValueError("admin token must be hexadecimal") from error
    if len(token) != ADMIN_SECRET_BYTES or not any(token):
        raise ValueError("admin token must contain 16 bytes and must not be all zero")
    return token


def render_include(admin_secret: bytes, certificate: str, private_key: str) -> str:
    secret_values = ", ".join(f"0x{value:02x}U" for value in admin_secret)
    return (
        "/* Generated per device. Never commit this file. */\n"
        f"static const uint8_t generated_admin_secret[] = {{{secret_values}}};\n"
        "static const uint8_t generated_tls_certificate[] =\n"
        f"    {json.dumps(certificate)};\n"
        "static const uint8_t generated_tls_private_key[] =\n"
        f"    {json.dumps(private_key)};\n"
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--certificate", type=Path, required=True)
    parser.add_argument("--private-key", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--credentials-output", type=Path, required=True)
    parser.add_argument("--admin-token", help="Reuse an existing 32-character hexadecimal token")
    arguments = parser.parse_args()

    certificate = read_pem(
        arguments.certificate, "-----BEGIN CERTIFICATE-----", MAX_CERTIFICATE_BYTES
    )
    private_key = read_pem(
        arguments.private_key, "-----BEGIN ", MAX_PRIVATE_KEY_BYTES
    )
    admin_secret = parse_admin_token(arguments.admin_token)
    admin_token = admin_secret.hex()

    atomic_private_write(
        arguments.output,
        render_include(admin_secret, certificate, private_key),
    )
    atomic_private_write(
        arguments.credentials_output,
        "\n".join(
            (
                "Motorized Desk device credentials",
                f"Administration token: {admin_token}",
                f"Provisioning WiFi password: {admin_token[:12]}",
                "",
                "Store this file offline. It cannot be recovered from the serial log.",
                "",
            )
        ),
    )


if __name__ == "__main__":
    main()
