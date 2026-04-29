# AGENTS.md

## Project

This repository contains ESP-IDF firmware for a custom two-actuator motorized L-shaped standing desk.

The firmware controls:
- Two 12 V linear actuators
- Two BTS7960 H-bridge drivers
- Four normally-closed limit switches
- UP / DOWN / optional CALIBRATE buttons
- Emergency stop input

The first production target is **Phase 1**: safe manual movement and calibration without height memory, display, WiFi, encoder feedback, PID, or automatic positioning.

This is a safety-conscious embedded project. Prefer conservative behavior over convenience.

---

## Hardware Summary

### Controller

- ESP32-S3 DevKitC-1
- ESP-IDF project
- Do not use Arduino APIs

### Actuators

- 2x 12 V linear actuators
- 1000 N each
- 500 mm stroke
- Approx. 14 mm/s
- No encoder feedback in Phase 1

### Motor Drivers

- 2x BTS7960
- One BTS7960 per actuator

Each motor has:
- RPWM
- LPWM
- Enable line if available
- Shared logic ground with ESP32
- Separate fused power branch

### Limit Switches

Use four normally-closed limit switches:

- Left upper limit
- Left lower limit
- Right upper limit
- Right lower limit

Electrical convention:

- Normal, not triggered: switch closed to GND
- Triggered or broken wire: input reads HIGH through pull-up

Therefore:

```text
LOW  = healthy / not triggered
HIGH = limit reached OR wiring fault
```

The firmware must treat HIGH as a stop condition.

### Buttons

- UP button
- DOWN button
- Optional CALIBRATE button

Recommended convention:

```text
LOW  = pressed
HIGH = released
```

### Emergency Stop

Emergency stop must immediately stop all motor outputs in firmware.

The physical emergency stop may also cut motor power directly. Firmware must still detect the input and enter safe state.

---

## Safety Rules

The following rules are mandatory.

1. Never drive RPWM and LPWM active at the same time for the same motor.
2. Always stop before changing direction.
3. Enforce configurable dead time before direction reversal.
4. If UP and DOWN are pressed simultaneously, stop all motion.
5. If emergency stop is active, stop all motion immediately.
6. If a matching limit switch is active, do not move further in that direction.
7. If a limit switch opens unexpectedly, treat it as limit reached or wiring fault.
8. If one actuator reaches an endstop earlier than the other, stop that actuator and allow the other actuator to continue only within the configured alignment timeout.
9. If alignment timeout expires, stop all motion and enter ERROR.
10. In ERROR state, all motor outputs must remain stopped.
11. No automatic positioning in Phase 1.
12. No movement shall happen automatically on boot.
13. Calibration must be explicit.
14. Host-side domain tests must verify all critical safety rules.

---

## Architecture Requirements

Use clean architecture.

The application logic must not call ESP-IDF APIs directly.

Required layering:

```text
app_main / platform startup
        ↓
application controller
        ↓
domain state machine
        ↓
HAL interfaces
        ↓
ESP-IDF hardware implementation
```

The domain layer must be testable on a host machine without ESP-IDF hardware.

### Directory Structure

```text
.
├── AGENTS.md
├── README.md
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── app_main.c
│   ├── platform/
│   ├── app/
│   ├── domain/
│   └── hal/
├── tests/
└── docs/
```

The structure may evolve, but separation of concerns must remain.

---

## Coding Standards

Use C for firmware unless the repository is intentionally migrated to C++.

General rules:

- Prefer simple, explicit C.
- Prefer static allocation.
- Avoid heap allocation unless clearly justified.
- Avoid hidden global mutable state in domain logic.
- Avoid blocking loops in domain logic.
- Avoid unbounded loops.
- Avoid large functions.
- Avoid magic numbers.
- Use enums for states, motor IDs, directions, and errors.
- Use `const` where possible.
- Validate function arguments.
- Keep ISR logic minimal if interrupts are introduced later.
- Debounce inputs either in the HAL/app layer or through explicit testable logic.
- Make timing configurable.
- Keep safety timing conservative.

### Formatting

- `snake_case` for functions and variables
- `UPPER_SNAKE_CASE` for constants/macros
- One responsibility per function
- No direct register manipulation unless isolated in platform code

---

## Domain Model

Minimum required states:

```c
typedef enum {
    DESK_STATE_IDLE,
    DESK_STATE_MOVING_UP,
    DESK_STATE_MOVING_DOWN,
    DESK_STATE_CALIBRATING_DOWN,
    DESK_STATE_ERROR
} desk_state_t;
```

Minimum required errors:

```c
typedef enum {
    DESK_ERROR_NONE,
    DESK_ERROR_EMERGENCY_STOP,
    DESK_ERROR_BOTH_BUTTONS_PRESSED,
    DESK_ERROR_ALIGNMENT_TIMEOUT,
    DESK_ERROR_LIMIT_CONFLICT,
    DESK_ERROR_INVALID_TRANSITION,
    DESK_ERROR_HARDWARE_FAULT
} desk_error_t;
```

The domain layer receives normalized semantic inputs and returns normalized motor commands.

---

## Timing Requirements

The following must be configurable:

```text
CONTROL_TICK_MS
DIRECTION_CHANGE_DEADTIME_MS
ALIGNMENT_TIMEOUT_MS
BUTTON_DEBOUNCE_MS
CALIBRATION_TIMEOUT_MS
```

Suggested defaults:

```text
CONTROL_TICK_MS              = 10-20 ms
DIRECTION_CHANGE_DEADTIME_MS = 300 ms
ALIGNMENT_TIMEOUT_MS         = 2000-3000 ms
BUTTON_DEBOUNCE_MS           = 30-50 ms
CALIBRATION_TIMEOUT_MS       = 45000 ms
```

---

## Phase 1 Feature Scope

Implement only:

- Manual hold-to-run UP
- Manual hold-to-run DOWN
- Independent left/right motor commands
- Limit switch safety
- Emergency stop handling
- Both-buttons-pressed safety behavior
- Direction-change dead time
- Alignment timeout
- Calibration-down routine
- Error state
- Documentation
- Unit tests

Do not implement:

- WiFi
- Web UI
- Home Assistant
- Display
- Touch screen
- Height memory
- PID
- Encoder
- Automatic height positioning
- OTA
- Cloud connectivity

Design extension points for those features, but do not implement them.

---

## Test-Driven Development Requirements

Use TDD for behavior changes. Before or alongside implementation, write host-side tests for:

- Boot and manual state behavior
- NC limit switch handling
- Independent axis handling
- Alignment timeout
- Direction dead time
- Calibration
- Error state
- Invalid and edge inputs
- Motor output safety

The domain state machine must not include:

- `gpio_set_level`
- `gpio_get_level`
- `vTaskDelay`
- ESP-IDF-specific types
- direct FreeRTOS dependencies

---

## Runtime Model

Recommended runtime:

- One main control task
- Fixed control tick, e.g. 10-20 ms
- Read inputs
- Normalize inputs
- Pass normalized inputs and elapsed time to domain controller
- Apply normalized outputs to hardware
- Log only on state/error changes, not every tick

Avoid printing every loop.

---

## Commit Rules

Use conventional commits. Commit often. Each commit should be small, reviewable, and buildable when possible.

Examples:

```text
chore: initialize ESP-IDF project structure
docs: add safety and electrical design notes
test: add motor axis safety tests
feat: add domain state machine skeleton
feat: implement manual hold-to-run movement
feat: add NC limit switch handling
feat: add emergency stop handling
feat: enforce direction change dead time
feat: add independent axis alignment timeout
feat: add calibration down routine
feat: add ESP-IDF platform HAL
docs: document build, pinout, safety, and testing
```

---

## Quality Gates

Before considering the project complete:

- Firmware builds successfully.
- Host/domain tests pass.
- Safety tests pass.
- No direct GPIO calls exist in domain logic.
- No automatic movement on boot.
- No state can command both directions of the same motor.
- Emergency stop always stops both motors.
- Alignment timeout is tested.
- Calibration timeout is tested.
- Limit switch fault behavior is tested.
- Documentation is updated.
- Pin mapping is explicit.
- Phase 1 limitations are documented.

---

## Safety Disclaimer

This firmware controls moving hardware capable of causing injury or property damage.

Firmware is not a substitute for:

- correct mechanical design
- physical emergency stop
- fuses
- correct cable sizing
- proper grounding
- correct actuator mounting
- lateral guides
- mechanical hard stops
- safe power distribution

When uncertain, fail safe and stop motion.
