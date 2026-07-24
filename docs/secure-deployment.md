# Secure Deployment

This procedure enables Secure Boot v2, flash encryption in release mode,
encrypted NVS, anti-rollback, and restricted ROM download mode. Some eFuse
changes are irreversible. Practice on the exact DevKit with motor power
physically disconnected and do not proceed without offline key backups.

## Required Assets

- one RSA-3072 Secure Boot signing key stored outside the repository;
- one per-device TLS certificate and matching private key;
- a trusted offline location for the generated credential record;
- ESP-IDF 6.0 and a stable USB-to-UART connection;
- the final `ESP32-S3-DevKitC-1U-N8R8`, not a substitute module.

Do not use a production signing key in CI or on a shared workstation. Losing
the key prevents future trusted updates. Leaking it permits malicious signed
images and requires retiring affected trust.

## Build

Generate the signing key once:

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
python -m espsecure generate-signing-key --version 2 \
  /secure/desk-signing.pem
chmod 600 /secure/desk-signing.pem
```

Build one device:

```sh
tools/build_release.sh \
  --signing-key /secure/desk-signing.pem \
  --tls-certificate /secure/desk-fullchain.pem \
  --tls-private-key /secure/desk-private-key.pem \
  --credentials-output /secure/desk-device-credentials.txt
```

The script verifies certificate/key matching, certificate lifetime, RSA key
integrity, bootloader/app signatures, and SHA-256 hashes. Generated C secrets
are deleted on exit. Review `build-release/SHA256SUMS`.

## Preflight

1. Engage E-stop and disconnect all three 12 V motor branches.
2. Power only the ESP32 and logic rail.
3. Verify the board/module/flash size and read current eFuses:

```sh
python -m espefuse --chip esp32s3 --port /dev/tty.usbserial-XXXX summary
```

4. Stop if Secure Boot/flash encryption state is unexpected or the chip is not
   the intended device.
5. Back up the signing key, TLS key/certificate, credential record, release
   hashes, source commit, and eFuse summary in separate protected locations.
6. Ensure power and USB cannot be interrupted during first boot.

## First Flash

Flash the signed bootloader and app produced by the release script, not the
unsigned default build artifacts:

```sh
python -m esptool --chip esp32s3 \
  --port /dev/tty.usbserial-XXXX \
  --before default-reset --after no-reset --no-stub \
  write-flash --flash-mode dio --flash-freq 80m --flash-size 8MB \
  0x0 build-release/bootloader/bootloader-signed.bin \
  0x10000 build-release/partition_table/partition-table.bin \
  0x1F000 build-release/ota_data_initial.bin \
  0x30000 build-release/motorized_desk-signed.bin
```

Reset once and allow the bootloader to complete first-boot encryption/eFuse
work without power loss. Keep motors disconnected. Capture the complete boot
log and verify `SAFE_START`, three-axis STOP, HTTPS startup, NVS initialization,
and no plaintext credential output.

Read eFuses again and compare them with `sdkconfig.release.defaults`. Confirm
Secure Boot v2, flash encryption release mode, restricted ROM download mode,
and anti-rollback state before restoring any motor branch.

## Commissioning and OTA

Complete logic-only, one-axis, three-axis, load, thermal, reset, E-stop,
disconnect, rollback, and soak gates in
[`commissioning.md`](commissioning.md). Use HTTPS OTA for normal updates.

Every release image must:

- use the same trusted signing key;
- carry a secure version not below the burned anti-rollback version;
- pass host/ESP-IDF CI and signature verification;
- preserve the partition layout unless a separately validated migration exists;
- retain a known-good rollback slot.

Increase the anti-rollback secure version only when intentionally revoking all
older images. An accidental increase cannot be undone.

## Recovery Boundary

Restricted ROM download mode is deliberately retained for authenticated,
encrypted recovery rather than disabling UART completely. Secure Boot prevents
untrusted code execution, but physical access can still cause denial of service
or destructive flash/eFuse operations.

Before any UART recovery, remove motor power and use Espressif's procedure for
the device's actual eFuse/encryption state. Never flash an unsigned bootloader,
plaintext secret partition, or an artifact from another device. A failed secure
recovery is not a reason to weaken Secure Boot or flash encryption.
