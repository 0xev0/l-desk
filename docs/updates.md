# Signed Updates and Recovery

## Trust Model

The production profile combines:

- RSA Secure Boot v2 for bootloader and applications;
- release-mode flash encryption;
- encrypted NVS key storage protected by flash encryption;
- A/B app partitions and pending-image rollback;
- anti-rollback secure version;
- authenticated HTTPS upload over a maintenance STOP inhibit.

The private signing key is the root of software trust. Keep it offline and
backed up. Each device also has a unique TLS key/certificate and administration
secret; never reuse another device's generated build include or credential
record.

## Build and Verify

Use only:

```sh
tools/build_release.sh \
  --signing-key /secure/desk-signing.pem \
  --tls-certificate /secure/desk-fullchain.pem \
  --tls-private-key /secure/desk-private-key.pem \
  --credentials-output /secure/desk-device-credentials.txt
```

Release artifacts are:

- `build-release/bootloader/bootloader-signed.bin`
- `build-release/motorized_desk-signed.bin`
- `build-release/partition_table/partition-table.bin`
- `build-release/ota_data_initial.bin`
- `build-release/SHA256SUMS`

Do not flash the unsigned `.bin` files that ESP-IDF emits before external
signing. Do not rebuild between verification and flashing.

## First Flash

Follow [`secure-deployment.md`](secure-deployment.md). First boot may burn
irreversible eFuses and encrypt flash. Motors must be physically disconnected,
power must be stable, and post-boot eFuse state must be verified.

## Web OTA

The authenticated console accepts a signed binary only while all axes are
stopped. The server:

1. verifies HTTPS origin, session, content type, declared size, and stopped
   state;
2. enables maintenance inhibit and waits for a published three-axis STOP;
3. writes bounded chunks to the inactive app partition;
4. requires exact length and successful `esp_ota_end()` security validation;
5. selects the new slot and reboots.

A receive/write/signature failure aborts the inactive update. Maintenance does
not release into a held physical or remote control.

## Confirmation, Rollback, and Anti-Rollback

A new app starts `PENDING_VERIFY` and is marked valid only after the control
task publishes coherent STOP and NVS/WiFi/HTTPS services initialize. Failure
requests rollback and reboot.

The secure version in `sdkconfig.release.defaults` prevents booting an image
below the eFuse anti-rollback version. Increase it only when intentionally
revoking all older releases. Preserve at least one signed, compatible,
non-revoked recovery image.

## Recovery

Recovery depends on actual eFuse state and may require pre-encrypted, signed
artifacts through restricted ROM download mode. Before UART work:

1. remove all motor power;
2. capture `espefuse summary`;
3. verify release hashes and signing identity;
4. follow Espressif's procedure for that exact secure state;
5. restore logic-only and HTTPS health before reconnecting any actuator.

Never disable signature checks, reduce secure version, reuse another device's
TLS material, or flash plaintext secrets to make recovery easier.
