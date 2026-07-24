# Three-Axis Motorized Desk Firmware

ESP-IDF firmware for a homelab L-shaped standing desk with three Hall-feedback
linear actuators and one BTS7960 driver per actuator. It supports local,
hybrid, and mobile-only hold-to-run control, downward calibration, synchronized
motion, fail-safe NC limits, presets, diagnostics, and signed A/B updates.

## Current Status

The firmware architecture, host tests, sanitizers, and ESP-IDF 6.0 build pass.
The physical desk is **not production ready** until the exact actuators, power
supply, BTS7960 modules, wiring, Hall scale, thermal behavior, and mechanical
frame pass [commissioning](docs/commissioning.md).

Implemented:

- Three independently controlled actuator axes
- `SAFE_START`, `UNREFERENCED`, manual UP/DOWN, calibration, and latched `ERROR`
- Six NC upper/lower limits with immediate activation and filtered release
- E-stop detection, both-buttons fault, and no movement on boot
- 300 ms direction reversal dead time and 500 ms soft start
- Hall quadrature counting through ESP-IDF PCNT
- Hall direction, missing feedback, stall, and skew detection
- Per-axis integer Hall scaling to micrometers with no floating point
- Deliberate calibration required after boot before normal movement
- PWM trim of the leading axis and hard stop at the configured skew threshold
- 750 ms endstop alignment window for all three arrival orders
- Static pinned control task and bounded asynchronous platform logging
- Runtime minimum-free-heap and dropped-log diagnostics for soak testing
- Tested local/hybrid/mobile-only command arbiter with exclusive 250 ms leases
- Versioned WiFi credentials in NVS with STA and recovery SoftAP infrastructure
- Authenticated responsive mobile UI and binary hold-to-run WebSocket control
- Three hold-to-run presets with bounded, CRC-protected NVS records
- Fixed diagnostic history with reset/error cause and runtime health metrics
- Signed OTA upload, dual app partitions, boot rollback, and coredump partition
- Maintenance inhibit during updates, including held-control release interlock
- Injectable HAL, host fake, watchdog, strict warnings, ASan, and UBSan tests
- Deliberate error clear: hold CALIBRATE for 2 seconds, then release

The WiFi web application is the primary controller. Physical UP/DOWN/CALIBRATE
buttons are optional in `MOBILE_ONLY`; the E-stop, six NC endstops, and Hall
feedback are never optional. See [network control](docs/networking.md),
[control sources](docs/control-sources.md), and [updates](docs/updates.md).

Not implemented: unattended automatic positioning, display, BLE, MQTT/Home
Assistant, TLS, NVS encryption, or hardware Secure Boot. These are not required
for the one-month release and must not bypass the existing command/safety
boundary if added.

## Target Hardware

- ESP32-S3-DevKitC-1U-N8R8 plus external 2.4 GHz antenna
- Three identical 12 V, 1000 N, 500 mm actuators with quadrature Hall outputs
- Three matching BTS7960/IBT-2 modules
- Six NC endstop switches
- UP, DOWN, and CALIBRATE buttons
- Dual-contact latching NC E-stop and DC-rated contactor
- Properly sized 12 V PSU, 12 V to 5 V regulator, main fuse, three motor fuses,
  and one logic fuse
- Enclosure, terminal blocks, ferrules, strain relief, and locking connectors

See [hardware](docs/hardware.md), [electrical design](docs/electrical-design.md),
and [pinout](docs/pinout.md) before connecting anything.

## Signed Firmware Build

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
idf.py set-target esp32s3
python -m espsecure generate-signing-key --version 2 \
  /absolute/private/path/desk-signing.private.pem
tools/build_signed.sh /absolute/private/path/desk-signing.private.pem
```

Generate the RSA-3072 key once, back it up securely, and never put it in this
repository. `tools/build_signed.sh` builds, signs the app image in place, and
verifies the signature. Signature verification on update derives trust from
the running signed app, so the first UART-flashed app must also be signed.
Flashing an unsigned app with this configuration aborts during startup.

Flash through the DevKit USB-to-UART port because GPIO19/20 are used for Hall:

```sh
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

Do not rebuild between signing and flashing. See [updates](docs/updates.md) for
A/B layout, OTA, rollback, and recovery.

## Host Tests

```sh
cmake -S tests -B build-host -DDESK_ENABLE_SANITIZERS=ON
cmake --build build-host --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-host --output-on-failure
```

The current suite has 25 CTest gates, including 23 host C executables, a mobile
HTTP contract test, and JavaScript syntax validation. Apple's ASan runtime does
not support LeakSanitizer. The control path performs no project-owned heap
allocation; heap stability still has to be measured on the ESP32 during soak.

## Safety

This firmware controls hardware capable of injury and property damage. It is
not a substitute for a rigid guided structure, hard stops, physical E-stop,
contactor, fuses, cable sizing, grounding, thermal validation, and cautious
commissioning. Never power motors from a PC USB port.
