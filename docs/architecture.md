# Architecture

The firmware uses a clean layered architecture:

```text
app_main / platform startup
        ↓
application runtime loop
        ↓
domain state machine
        ↓
HAL interfaces
        ↓
ESP-IDF hardware implementation
```

The domain layer contains no ESP-IDF GPIO, FreeRTOS, or hardware-specific calls. It receives normalized inputs and elapsed time, then returns normalized motor commands and controller state.

## Runtime Loop

The application runs a fixed-period control task:

1. Read raw GPIO inputs through the platform HAL.
2. Normalize electrical levels into semantic inputs.
3. Filter safety inputs in the app layer.
4. Tick the domain controller with elapsed milliseconds.
5. Apply motor outputs through the platform HAL.
6. Log state or error changes only.
7. Sleep until the next control tick.

The loop is implemented in `main/app/desk_app.c`. ESP-IDF and FreeRTOS calls are kept in the app/platform layers, not in `main/domain`.

## Input Filtering

`main/app/desk_input_filter.c` conditions safety inputs before they reach the domain controller. Emergency stop and NC limit activation are immediate. Clearing those inputs requires a stable inactive reading for `DESK_SAFETY_INPUT_RELEASE_DEBOUNCE_MS`, currently 50 ms. This prevents contact bounce from re-enabling motion while preserving fail-safe stop behavior.

Manual UP/DOWN/CALIBRATE buttons pass through the app input filter. Manual movement debounce is handled by the domain controller so hold-to-run behavior remains deterministic in host tests.

## State Machine

Phase 1 states:

- `DESK_STATE_IDLE`
- `DESK_STATE_MOVING_UP`
- `DESK_STATE_MOVING_DOWN`
- `DESK_STATE_CALIBRATING_DOWN`
- `DESK_STATE_ERROR`

All states command stopped motors unless there is an explicit, currently valid manual or calibration request.

`DESK_STATE_ERROR` latches motion inhibited. Phase 1 clears errors only through `desk_controller_request_clear_error()` or reboot. Clearing an error does not itself command motion.

Manual UP/DOWN requests are debounced in the controller before motion starts. Stop, emergency, both-buttons, and limit safety paths are not delayed by debounce.

Calibration starts only from idle and only when neither UP nor DOWN is pressed. Ambiguous calibration requests enter `DESK_STATE_ERROR` with `DESK_ERROR_INVALID_TRANSITION`.

Alignment timeout and calibration timeout are separate domain errors so logs can distinguish side mismatch from a calibration routine that never reached both lower limits.

Direction reversal is detected before manual debounce. The old direction stops immediately and the configured dead-time starts at the reversal request; debounce applies to starting ordinary manual movement, not to delaying a safety stop.

## HAL Boundaries

- `main/domain/*`: pure C state machine and motor output safety helpers.
- `main/app/desk_input_filter.*`: host-testable input conditioning for safety signals.
- `main/hal/*`: small platform-independent interface types.
- `main/platform/*`: ESP-IDF GPIO, timing, and BTS7960 implementation.
- `tests/fake_gpio.*` and `tests/fake_time.*`: host-side fakes for future app-level tests.

The domain layer has no direct `gpio_*`, `vTaskDelay`, or ESP-IDF dependency.

Build warning policy is intentionally strict. Host tests and the ESP-IDF main component treat common warnings as errors.

## Output Safety Helpers

`main/domain/motor_axis.*` owns the pure mapping from logical motor commands to BTS7960 RPWM/LPWM/enable outputs. The helper distinguishes basic safety from stricter coherence:

- Safe output never drives RPWM and LPWM together.
- Coherent output also requires STOP to de-energize every output and motion commands to assert enable with exactly one direction.

The platform BTS7960 driver checks command validity and output coherence before touching GPIO. Invalid or incoherent output fails safe to STOP.

## Extension Points

Future features may add position sensors, display, persistent presets, or connectivity, but those features must enter through application services that call the domain API. They must not bypass safety logic or directly command GPIO outputs.

## Phase 2 Feedback Extension

`main/domain/desk_feedback.*` defines normalized feedback inputs and diagnostics without selecting hardware. It can report feedback OK, missing, or faulted, including left/right axis disagreement beyond a configured threshold.

`main/hal/desk_feedback_source.h` defines the small provider interface future platform drivers can implement. Tests use `tests/fake_feedback.*` so feedback behavior can be exercised on the host before ESP32 sensor drivers exist.

The feedback monitor adds temporal plausibility checks between valid samples. A missing or faulted sample resets monitor history, and an implausible position step is reported as a feedback fault rather than being smoothed or corrected.

`main/app/desk_feedback_observer.*` wraps a feedback source and the domain monitor for app-level polling. It is host-tested with the fake provider and is ready for future platform sensor drivers, but it is not connected to motor command decisions.

`main/platform/feedback_none_source.*` is the current runtime source. It reports feedback missing until real sensor hardware is selected and implemented. The app loop polls the observer and logs feedback diagnostic changes only.

Feedback is observational at this stage. It is not connected to motor command decisions, automatic positioning, or calibration behavior.
