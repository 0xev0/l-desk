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
3. Tick the domain controller with elapsed milliseconds.
4. Apply motor outputs through the platform HAL.
5. Log state or error changes only.
6. Sleep until the next control tick.

The loop is implemented in `main/app/desk_app.c`. ESP-IDF and FreeRTOS calls are kept in the app/platform layers, not in `main/domain`.

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
- `main/hal/*`: small platform-independent interface types.
- `main/platform/*`: ESP-IDF GPIO, timing, and BTS7960 implementation.
- `tests/fake_gpio.*` and `tests/fake_time.*`: host-side fakes for future app-level tests.

The domain layer has no direct `gpio_*`, `vTaskDelay`, or ESP-IDF dependency.

## Extension Points

Future features may add position sensors, display, persistent presets, or connectivity, but those features must enter through application services that call the domain API. They must not bypass safety logic or directly command GPIO outputs.
