# Phase 2 Feedback Requirements

Phase 2 adds measurement and diagnostics around actuator position or synchronization. It must not weaken Phase 1 safety behavior.

No Phase 2 firmware feature should be implemented until the feedback hardware is selected and bench-tested.

## Goals

- Detect actuator drift with enough confidence to support future assisted positioning.
- Keep manual hold-to-run movement usable when feedback is absent but safe.
- Add diagnostics for sensor wiring faults, impossible readings, and disagreement between axes.
- Preserve the Phase 1 controller as the final authority for motor outputs.

## Non-Goals

- No automatic height positioning in the first feedback milestone.
- No PID control until sensor reliability is proven.
- No WiFi, display, Home Assistant, OTA, or remote operation.
- No direct GPIO or ADC access from domain logic.

## Candidate Feedback Options

| Option | Benefits | Risks |
| --- | --- | --- |
| Hall pulse sensors | Simple incremental movement tracking if actuators support them | Needs homing, can lose counts, needs interrupt hygiene |
| Linear potentiometers | Absolute-ish position measurement | ADC noise, mechanical mounting, wear, calibration drift |
| Magnetic linear encoders | Better precision and isolation | More hardware complexity, cost, alignment sensitivity |
| External tilt/level sensing | Can detect tabletop mismatch | Does not directly measure actuator stroke |

## Required Hardware Decisions

Before implementation, decide:

- Sensor type per axis.
- Electrical interface and voltage levels.
- ESP32-S3 pins and ADC/interrupt constraints.
- Failure behavior for disconnected, stuck, noisy, or disagreeing sensors.
- Whether feedback is safety-critical or advisory in the first milestone.
- Calibration method and acceptable measurement error.

## Architecture Constraints

Feedback must enter through new HAL interfaces, then be normalized before reaching domain logic.

```text
platform sensor driver
        ↓
feedback HAL interface
        ↓
application normalization/filtering
        ↓
domain diagnostics/state machine
```

The domain layer may receive normalized feedback structs later, but it must remain host-testable and free of ESP-IDF dependencies.

## Safety Rules For Phase 2

- Emergency stop, limit switches, direction dead-time, and ERROR state override all feedback behavior.
- Sensor faults must never cause motion by themselves.
- Sensor disagreement must fail stopped for any mode that depends on feedback.
- Manual movement may remain available only if the configured policy treats feedback as advisory.
- Clearing a sensor fault must not automatically start movement.

## Test Requirements

Add host tests before implementing feedback behavior:

- Feedback absent at boot.
- Sensor value stuck high or low.
- Sensor value jumps implausibly.
- Left/right disagreement exceeds threshold.
- Feedback fault during upward movement.
- Feedback fault during downward movement.
- Feedback fault during calibration.
- Recovery from feedback fault requires explicit clear action.
- Large elapsed time and large count deltas do not overflow.

## First Implementation Slice

The first code slice is intentionally small:

1. Add normalized feedback types with no platform dependencies.
2. Add a fake feedback provider for host tests.
3. Add diagnostics that can report `feedback_ok`, `feedback_missing`, or `feedback_fault`.
4. Do not use feedback to command movement yet.

That keeps Phase 2 observational before it becomes controlling.

Status: implemented as a host-tested observational domain module. Feedback is not wired into motor control.

The current monitor can detect:

- Missing feedback.
- Axis-reported feedback faults.
- Left/right position disagreement.
- Implausible position jumps between valid samples.

The app layer now has a host-tested feedback observer that reads through `desk_feedback_source_t` and feeds the domain monitor. It remains unused by the control loop until real sensor hardware and policy decisions are available.
