# L-Desk Firmware

[![CI](../../actions/workflows/ci.yml/badge.svg)](../../actions/workflows/ci.yml)
![Host tests](docs/test-status.svg)
[![ESP-IDF 6.0](https://img.shields.io/badge/ESP--IDF-6.0-E7352C)](https://docs.espressif.com/projects/esp-idf/en/v6.0/)
[![License: PolyForm Noncommercial](https://img.shields.io/badge/license-PolyForm%20Noncommercial-4B5563)](LICENSE)

Safety-conscious ESP-IDF firmware for a three-actuator L-shaped standing desk.
It combines local or phone-first hold-to-run control, Hall synchronization,
fail-safe NC endstops, presets, diagnostics, and signed A/B updates without a
custom PCB or cloud dependency.

> [!CAUTION]
> This project controls heavy moving hardware. It is not a certified safety
> controller. Never replace a physical E-stop, DC contactor, fuses, hard stops,
> rigid guides, correct wiring, grounding, or commissioning with firmware.

![L-Desk mobile web control console](docs/control-console.png)

## Release Status

| Area | Software | Physical validation |
| --- | --- | --- |
| P1 manual control and fail-safe limits | Implemented and host-tested | Required on final wiring |
| P2 Hall feedback and three-axis synchronization | Implemented and host-tested | Hall scale, skew, and load tests required |
| P3 three hold-to-run presets | Implemented and host-tested | Target tolerance test required |
| P4 diagnostics, signed A/B OTA, rollback | Implemented and build-tested | Power-loss and rollback tests required |
| P5A WiFi, HTTPS/WSS, mobile web controller | Implemented and integration-tested | LAN, certificate, and disconnect tests required |
| P5B BLE, MQTT, Home Assistant | Deliberately deferred | Not applicable |

The source, tests, and secure ESP-IDF 6.0 profile build successfully. The desk
is **not hardware release-ready** until every gate in
[commissioning](docs/commissioning.md) passes with the exact actuators,
drivers, PSU, frame, wiring, and enclosure. Release tags are forbidden before
that evidence exists.

Firmware identifies itself from the Git revision through ESP-IDF and exposes
that value in the authenticated API and console. The
[latest GitHub release](../../releases/latest), when one exists, is the
canonical public version; test and workflow badges are generated from CI
results rather than manually entered counts.

## What Is Implemented

- Three independently controlled BTS7960 axes with validated STOP/enable/PWM
  sequencing and mutually exclusive directions.
- `SAFE_START`, `UNREFERENCED`, `IDLE`, motion, calibration, and latched `ERROR`
  states with no movement on boot or error clear.
- Six NC endstops, monitored E-stop, both-buttons fault, limit conflict, 300 ms
  reversal dead time, soft start, and bounded endstop alignment.
- ESP-IDF PCNT quadrature Hall capture, per-axis integer scaling, direction and
  stall checks, warning trim, and hard skew stop.
- Local-only, hybrid, and remote-only control. Physical motion buttons are
  optional; E-stop, Hall feedback, and endstops are not.
- 100 ms WebSocket command heartbeat with a 250 ms lease, replay protection,
  one remote owner, and STOP on release, blur, page hide, timeout, or disconnect.
- Responsive English web console with explicit HTTPS/API/control-channel
  status, accessible dialogs, safe error clear, provisioning, diagnostics,
  presets, and signed OTA.
- Per-device credentials, HTTPS/WSS only, secure cookies, exact origin checks,
  login throttling, encrypted NVS, Secure Boot v2, flash encryption,
  anti-rollback, and restricted ROM download mode in the production profile.
- Pure C domain, injected HAL, fixed-size state, static control queues, bounded
  work, strict warnings, ASan/UBSan, integration tests, and exhaustive safety
  properties.

## Reference Hardware

- ESP32-S3-DevKitC-1U-N8R8 and external 2.4 GHz antenna
- Three identical 12 V linear actuators with 500 mm stroke and quadrature Hall
- Three matching BTS7960/IBT-2 modules
- Six NC upper/lower endstops
- Optional UP, DOWN, and CALIBRATE buttons
- Dual-contact latching NC E-stop and correctly rated DC contactor
- Sized 12 V PSU, 12 V to 5 V regulator, main/motor/logic fuses
- Rigid guided frame, enclosure, terminal blocks, ferrules, strain relief, and
  locking signal connectors

Read [hardware](docs/hardware.md), [electrical design](docs/electrical-design.md),
and the provisional [pinout](docs/pinout.md) before purchasing or wiring.
Never power an actuator or BTS7960 motor rail from PC USB.

## First Boot Without Buttons

Buttons are active-low inputs with internal pull-ups. An unconnected button is
therefore read as released, so a buttonless installation does not remain stuck
in `SAFE_START`.

1. Build a per-device release and keep its generated credential record offline.
2. Commission Secure Boot and flash encryption with motor power physically
   disconnected.
3. Join the protected `Desk-XXXXXX` provisioning network using that record.
4. Open the provisioned HTTPS hostname/IP trusted by the certificate, sign in,
   and configure the home WiFi.
5. Hold **CAL** in the web console until all three lower limits establish the
   Hall reference.
6. Select **Remote only** if local motion buttons will not be installed.

Hybrid mode is the first-boot default. Error clear from the web console
revalidates every safety input, returns to `SAFE_START`, and never moves the
desk automatically.

See [networking](docs/networking.md) and
[control sources](docs/control-sources.md) for the complete behavior.

## Host Quality Gates

```sh
cmake -S tests -B build-host -DDESK_ENABLE_SANITIZERS=ON
cmake --build build-host --parallel
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-host --output-on-failure
```

The test badge is generated from CTest JUnit output, and CI rejects a stale
badge. The suite covers pure domain behavior, fake-HAL application flows,
output sequencing, security codecs, persistent records, HTTP integration,
embedded web contracts, and exhaustive input properties. Coverage and hardware
gates are documented in [testing](docs/testing.md).

## Secure Firmware Build

The production flow requires a long-lived RSA-3072 signing key, a per-device TLS
certificate/key, and an offline destination for generated credentials:

```sh
tools/build_release.sh \
  --signing-key /secure/desk-signing.pem \
  --tls-certificate /secure/desk-fullchain.pem \
  --tls-private-key /secure/desk-private-key.pem \
  --credentials-output /secure/desk-device-credentials.txt
```

The script builds the production profile, signs and verifies bootloader/app
artifacts, writes SHA-256 checksums, and removes generated secrets from the
source tree. It does **not** burn eFuses or flash hardware.

Secure first flash is partly irreversible. Follow
[secure deployment](docs/secure-deployment.md) exactly; do not substitute a
plain `idf.py flash` command from an older guide.

## HTTPS Deployment

The ESP32 always serves HTTPS/WSS. Two practical trust models are supported:

- **Direct LAN:** issue a long-lived per-device certificate from a private CA
  trusted by the phone and local DNS.
- **Reverse proxy:** keep the device on an isolated LAN with its private
  certificate and place Caddy/Nginx in front with a public hostname and
  automatically renewed Let's Encrypt certificate. Proxy HTTPS and WSS; never
  expose the ESP32 directly to the Internet.

Let's Encrypt does not issue certificates for `localhost`, and short-lived
public certificates should not be baked into firmware. See
[networking](docs/networking.md) for the topology and trust boundaries.

## Architecture

```text
physical buttons / HTTPS-WSS mobile client
                 |
        bounded command arbiter
                 |
         application controller
                 |
      pure three-axis state machine
                 |
          injected HAL contract
                 |
 GPIO / LEDC / PCNT / NVS / WiFi / OTA
```

Domain and HAL code contain no ESP-IDF, FreeRTOS, GPIO, or heap dependency.
Networking can submit only high-level leased actions and cannot address a motor
or pin. Read [architecture](docs/architecture.md) and
[safety](docs/safety.md) for invariants and failure behavior.

## Community

- [Contributing](CONTRIBUTING.md)
- [Support](SUPPORT.md)
- [Security policy](SECURITY.md)
- [Code of conduct](CODE_OF_CONDUCT.md)
- [Governance](GOVERNANCE.md)
- [Changelog](CHANGELOG.md)
- [Release process](RELEASING.md)
- [Firmware bug form](../../issues/new?template=bug_report.yml)
- [Feature proposal form](../../issues/new?template=feature_request.yml)
- [Hardware compatibility form](../../issues/new?template=hardware_compatibility.yml)

## License

Copyright 2026 Alvaro Diago de Aguilar.

This project is source-available under the
[PolyForm Noncommercial License 1.0.0](LICENSE). Personal, hobby, educational,
research, charitable, and other permitted noncommercial uses are welcome.
Commercial use is not granted. Because of that field-of-use restriction, this
license is not OSI-approved open source.
