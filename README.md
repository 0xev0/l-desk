# Three-Axis Motorized Desk Firmware

ESP-IDF firmware for a homelab L-shaped standing desk with three Hall-feedback
linear actuators and one BTS7960 driver per actuator. The current release is a
local, hold-to-run controller with downward calibration, synchronized motion,
fail-safe NC limits, and a monitored emergency stop.

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
- Injectable HAL, host fake, watchdog, strict warnings, ASan, and UBSan tests
- Deliberate error clear: hold CALIBRATE for 2 seconds, then release

The local HTTP/WebSocket transport, bounded control queue, credential store,
WiFi lifecycle, and mobile web controller are connected. See [network
control](docs/networking.md) and [control sources](docs/control-sources.md).

Not implemented: presets, automatic positioning, display, BLE, Home Assistant,
or OTA.

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

## Firmware Build

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
idf.py set-target esp32s3
idf.py build
```

The activation script defines `idf.py` as an interactive shell alias. In a
non-interactive shell, invoke its Python entry point directly if necessary.

Flash through the DevKit USB-to-UART port because GPIO19/20 are used for Hall:

```sh
idf.py -p /dev/tty.usbserial-XXXX flash monitor
```

## Host Tests

```sh
cmake -S tests -B build-host -DDESK_ENABLE_SANITIZERS=ON
cmake --build build-host --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-host --output-on-failure
```

Apple's ASan runtime does not support LeakSanitizer. The control path performs
no project-owned heap allocation; heap stability still has to be measured on
the ESP32 during the soak test.

## Safety

This firmware controls hardware capable of injury and property damage. It is
not a substitute for a rigid guided structure, hard stops, physical E-stop,
contactor, fuses, cable sizing, grounding, thermal validation, and cautious
commissioning. Never power motors from a PC USB port.
