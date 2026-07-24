# Signed Updates and Recovery

## Trust Model

The project verifies RSA-3072 signatures when an app is written through ESP-IDF
OTA APIs. Hardware Secure Boot and flash encryption are deliberately disabled:
this protects network updates from untrusted images but does not protect
against someone with physical UART/flash access.

The running signed app supplies the public-key trust for the next OTA image.
Therefore the first serial-flashed app must be signed with the same private key
used for later releases. Keep that key outside the repository and back it up
offline. Losing it prevents trusted OTA updates; leaking it lets an attacker
sign accepted updates.

## Build and Sign

Generate one key outside the repository:

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
python -m espsecure generate-signing-key --version 2 \
  /absolute/private/path/desk-signing.private.pem
```

Build, sign in place, and verify:

```sh
tools/build_signed.sh /absolute/private/path/desk-signing.private.pem
```

The upload/flash artifact is `build/motorized_desk.bin`. Do not run another
build after signing because the build target intentionally produces an
unsigned artifact for external signing.

## First Serial Flash

With the signed artifact still present:

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

GPIO19/20 are Hall inputs, so use the DevKit USB-to-UART connector. Verify the
boot log reaches `SAFE_START`; an unsigned initial app aborts during ESP-IDF
signature startup checks.

## Mobile OTA

Authenticate in the local web UI, choose the signed `.bin`, and update only
while the desk is stopped. The server:

1. rejects unauthenticated, cross-origin, non-binary, empty, or oversized data;
2. queues maintenance inhibit and waits for all three outputs to report STOP;
3. writes bounded 1 KiB chunks to the inactive 2.5 MiB app partition;
4. requires exact declared length and successful `esp_ota_end()` signature
   verification;
5. sets the new boot partition and reboots.

If receive, flash, or signature verification fails, the inactive write is
aborted. Maintenance remains until physical controls are released, preventing
a held button from moving the desk immediately after the failed update.

## Confirmation and Rollback

The new image starts as `PENDING_VERIFY`. It is marked valid only after:

- the control task publishes a coherent three-axis STOP vector; and
- WiFi/NVS/HTTP services initialize successfully.

Failure requests rollback and reboot. Power loss before selecting the new slot
leaves the old slot active. Power loss after selection but before confirmation
causes the bootloader rollback flow on the following boot.

## Recovery

Keep USB-to-UART physically accessible without exposing motor terminals.
Before serial recovery:

1. engage E-stop and disconnect all three motor power branches;
2. build/sign a known-good commit with the established key;
3. flash bootloader, partition table, OTA metadata, and signed app;
4. verify logic-only startup and diagnostics before restoring motor power.

Do not enable hardware Secure Boot or burn related eFuses until this recovery
procedure has been exercised repeatedly on the exact DevKit.
