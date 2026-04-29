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

## State Machine

Phase 1 states:

- `DESK_STATE_IDLE`
- `DESK_STATE_MOVING_UP`
- `DESK_STATE_MOVING_DOWN`
- `DESK_STATE_CALIBRATING_DOWN`
- `DESK_STATE_ERROR`

All states command stopped motors unless there is an explicit, currently valid manual or calibration request.

## Extension Points

Future features may add position sensors, display, persistent presets, or connectivity, but those features must enter through application services that call the domain API. They must not bypass safety logic or directly command GPIO outputs.
