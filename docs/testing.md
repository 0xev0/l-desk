# Testing

## Host Suite

Twenty-five CTest gates cover domain, application, input filtering, fake HAL,
BTS7960 sequencing, network codecs/security, the mobile HTTP contract, web
JavaScript syntax, and output invariants without ESP-IDF hardware.

```sh
cmake -S tests -B build-host -DDESK_ENABLE_SANITIZERS=ON
cmake --build build-host --parallel
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-host --output-on-failure
```

The suite includes 2,048 combinations of six limit states plus UP, DOWN,
CALIBRATE, E-stop, and hardware-fault inputs. Every error output must stop all
three axes; every logical motor command must map to mutually exclusive RPWM and
LPWM.

## Coverage

Configure a Clang coverage build:

```sh
cmake -S tests -B build-coverage -DDESK_ENABLE_COVERAGE=ON
cmake --build build-coverage --parallel
mkdir -p /tmp/motorized-desk-coverage
LLVM_PROFILE_FILE=/tmp/motorized-desk-coverage/%p.profraw \
  ctest --test-dir build-coverage --output-on-failure
```

Merge with `llvm-profdata` and report with `llvm-cov` over
all host-testable domain, application, and output-sequencing modules. The
current result is at least 96.7% lines, 84.6% branches, and 100% functions.
Keep line coverage at or above 95% and all safety transitions
explicitly tested.

## Covered Behavior

- `SAFE_START`, held boot buttons, manual hold-to-run, and immediate release
- `UNREFERENCED`, calibration-required boot, and integer position scaling
- Three-axis upper/lower limits and all six endstop arrival orders
- Alignment and calibration timeout
- Long-hold calibration, abort, and already-at-limit cases
- E-stop, both buttons, limit conflict, read/write/zero/watchdog failures
- Direction reversal and dead time
- Immediate safety activation and filtered release bounce
- Hall missing, wrong direction, stall, warning trim, and hard skew
- Invalid time, arguments, configuration, and motor output vectors
- BTS7960 STOP/start/reversal ordering and output-register readback invariants
- Local/hybrid/mobile-only arbitration, exclusive lease, replay, expiry,
  preemption, and mobile-only error clear
- Fixed WebSocket command framing, byte order, malformed lengths, reserved
  fields, actions, sequences, and TTL bounds
- Administration-token parsing, constant-time session lookup, login rate
  limiting, fixed session capacity, expiry, duplicate randomness, and logout
- WiFi SSID/PSK boundary lengths, control-character rejection, and every
  persisted control mode
- Percent-decoded provisioning forms, duplicate-key rejection, exact cookies,
  same-origin enforcement, and short/overlong authentication secrets
- Explicit long-hold/release error clear without automatic motion
- Preset capture/storage corruption, hold-to-run target crossing, no hunting,
  invalid feedback, and buttonless mobile preset operation
- Maintenance inhibit, held-control release interlock, diagnostic CRC/ring
  recovery, and stopped OTA boot-health predicate
- HTTP asset/login/status/mode/preset/provision/diagnostic/update contract and
  JavaScript syntax
- Deterministic repeated ticks and saturating timing arithmetic

## ESP-IDF Build

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
idf.py build
```

The main component treats format, shadow, conversion, signed conversion, and
enum-switch warnings as errors. Host builds additionally use pedantic and
undefined-macro warnings.

## Hardware Tests Still Required

Host tests cannot prove GPIO voltage, clone BTS timing, PCNT noise immunity,
contactor wiring, thermal margin, Hall scale, actuator mismatch, EMC behavior,
or mechanical skew. Complete the progressive tests in `commissioning.md` and
record the exposed current/minimum free heap, task stack high-water marks, and
dropped-log counters during the 8 hour soak. Confirm that the statically
allocated control task maintains its 10 ms cadence while diagnostics are being
emitted.

The Python HTTP test runs a deterministic contract double of the ESP-IDF
service. It catches mobile/API drift but does not exercise lwIP, the ESP-IDF
HTTP server, WiFi radio, flash writes, signature hardware path, or real
WebSocket timing. Those remain target-integration commissioning gates.
