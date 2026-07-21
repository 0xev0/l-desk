# AGENTS.md

## Project

This repository contains ESP-IDF C firmware for a custom three-actuator
motorized L-shaped standing desk. It is a safety-conscious homelab project,
not a certified commercial safety controller. Prefer a conservative stop over
convenience.

## Current Release

The first hardware release includes:

- Three identical 12 V linear actuators with quadrature Hall feedback
- Three BTS7960 drivers, one per actuator
- Six normally-closed upper/lower endstops
- UP, DOWN, and CALIBRATE controls
- Monitored dual-contact E-stop with independent physical motor power cut
- Manual hold-to-run, downward calibration, and Hall synchronization

Do not add presets, automatic positioning, WiFi, BLE, OTA, display, web UI,
Home Assistant, or persistent storage unless the task explicitly advances the
documented roadmap.

## Mandatory Safety Rules

1. Boot and reset keep all enables inactive and enter `SAFE_START`.
2. Never activate RPWM and LPWM together on one axis.
3. Stop before enable, initialization, or direction change.
4. Enforce the configured reversal dead time.
5. E-stop, hardware faults, feedback faults, and invalid outputs stop all axes.
6. UP plus DOWN is a latched error.
7. An NC endstop HIGH means reached or broken wire and blocks that direction.
8. Opposite travel away from a single active endstop remains allowed.
9. Upper plus lower active on one axis is a hardware conflict.
10. An axis reaching an endstop stops immediately; remaining axes get only the
    configured alignment interval.
11. Calibration requires a deliberate CALIBRATE hold and aborts on release.
12. `ERROR` always outputs STOP and requires an explicit long-hold/release clear
    or reboot.
13. Invalid Hall data, wrong direction, stall, or excessive skew stops all axes.
14. Network features may submit high-level requests later but never touch GPIO
    or bypass the local safety state machine.

## Electrical Semantics

NC safety inputs use pull-ups:

```text
LOW  = closed, healthy, inactive
HIGH = open, active, or broken wire
```

Buttons are active low. Hall signals must be open-collector with 3.3 V pull-ups
or otherwise proven 3.3 V compatible. Never apply 5 V to an ESP32 GPIO.

## Architecture

```text
app_main / runtime
        -> injected application HAL
        -> pure domain state machine
        -> normalized three-axis outputs
        -> ESP-IDF GPIO / LEDC / PCNT platform
```

- `main/domain`: pure C, fixed-size data, no ESP-IDF or FreeRTOS.
- `main/app`: orchestration, input release filtering, and error-clear gesture.
- `main/hal`: platform-independent injected interface.
- `main/platform`: ESP-IDF GPIO, LEDC, PCNT, time, watchdog, and logging.
- `tests`: host fake and deterministic safety tests.

The control path is O(`DESK_AXIS_COUNT`), statically allocated, non-blocking in
the domain, and receives absolute monotonic `uint64_t now_ms`.

## Coding Rules

- Use C, explicit fixed-width types, arrays sized by `DESK_AXIS_COUNT`, and
  `const` where appropriate.
- Avoid heap allocation, hidden mutable globals, blocking domain logic,
  unbounded loops, floating point, and ignored return values.
- Keep GPIO numbers and hardware tuning in `main/platform/board_config.h`.
- Validate complete output vectors before applying any motor output.
- Propagate `esp_err_t`; fail stopped on platform errors.
- Add or update host tests before or with every behavior change.
- Keep warnings as errors and use conventional commits.
- Do not weaken tests or safety timing to make a hardware problem disappear.

## Quality Gates

- Host tests pass with ASan and UBSan.
- ESP-IDF 6.0 builds for `esp32s3` with warnings as errors.
- Domain and HAL contain no direct ESP-IDF calls.
- Coverage remains at least 95% for domain and application lines.
- All error paths stop three motors and output validation rejects incoherence.
- No project-owned heap allocation exists in the control path.
- Documentation and commissioning gates match the exact hardware.
- A release tag is forbidden until physical commissioning and soak tests pass.

## Physical Constraints

Actuators provide vertical force only. A rigid frame and linear guides carry
lateral loads and prevent racking. Firmware cannot replace mechanical hard
stops, fuses, a DC-rated contactor, safe mains wiring, grounding, or thermal
engineering.
