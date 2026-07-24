#!/usr/bin/env python3

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "tools" / "generate_build_secrets.py"
CERTIFICATE = """-----BEGIN CERTIFICATE-----
VEVTVA==
-----END CERTIFICATE-----
"""
PRIVATE_KEY_LABEL = "PRIVATE KEY"
PRIVATE_KEY = (
    f"-----BEGIN {PRIVATE_KEY_LABEL}-----\n"
    "VEVTVA==\n"
    f"-----END {PRIVATE_KEY_LABEL}-----\n"
)


class BuildSecretsTest(unittest.TestCase):
    def test_generates_private_per_device_inputs_without_logging_the_token(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            certificate = root / "certificate.pem"
            private_key = root / "private-key.pem"
            include = root / "desk_build_secrets.inc"
            credentials = root / "device-credentials.txt"
            certificate.write_text(CERTIFICATE, encoding="ascii")
            private_key.write_text(PRIVATE_KEY, encoding="ascii")

            completed = subprocess.run(
                (
                    sys.executable,
                    GENERATOR,
                    "--certificate",
                    certificate,
                    "--private-key",
                    private_key,
                    "--output",
                    include,
                    "--credentials-output",
                    credentials,
                    "--admin-token",
                    "0123456789abcdef0123456789abcdef",
                ),
                check=True,
                capture_output=True,
                text=True,
            )

            self.assertEqual(completed.stdout, "")
            self.assertEqual(completed.stderr, "")
            self.assertEqual(os.stat(include).st_mode & 0o777, 0o600)
            self.assertEqual(os.stat(credentials).st_mode & 0o777, 0o600)
            generated = include.read_text(encoding="utf-8")
            record = credentials.read_text(encoding="utf-8")
            self.assertIn("generated_tls_certificate", generated)
            self.assertIn("generated_tls_private_key", generated)
            self.assertIn("Administration token: 0123456789abcdef", record)
            self.assertIn("Provisioning WiFi password: 0123456789ab", record)

    def test_rejects_invalid_token_and_pem_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            invalid = root / "invalid.pem"
            invalid.write_text("not-pem", encoding="ascii")
            completed = subprocess.run(
                (
                    sys.executable,
                    GENERATOR,
                    "--certificate",
                    invalid,
                    "--private-key",
                    invalid,
                    "--output",
                    root / "output.inc",
                    "--credentials-output",
                    root / "credentials.txt",
                    "--admin-token",
                    "00",
                ),
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse((root / "output.inc").exists())


if __name__ == "__main__":
    unittest.main()
