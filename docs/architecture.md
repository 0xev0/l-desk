# Architecture

## Layers

```text
app_main
  -> desk_app_run (ESP runtime loop)
  -> desk_app_step (platform-independent orchestration)
  -> desk_controller_tick (pure domain)
  -> normalized desk_outputs_t
  -> desk_platform_t / ESP-IDF backend
```

`desk_controller_tick()` has no ESP-IDF, GPIO, PCNT, logging, heap, or FreeRTOS
dependency. It receives normalized inputs and absolute monotonic `uint64_t`
time, then updates a caller-owned `desk_controller_t`.

Three axes are fixed at compile time through `DESK_AXIS_COUNT = 3`. Per-axis
state is held in static arrays, making each control tick O(3) with bounded work.

## Runtime

Every nominal 10 ms iteration:

1. Read normalized controls, six limits, and three Hall counters through HAL.
2. Activate limits/E-stop immediately and debounce only their release for 50 ms.
3. Evaluate the state machine using actual monotonic time.
4. Validate the complete three-axis output vector.
5. Apply STOP/LEDC/enable sequences through each BTS7960 driver.
6. Service the task watchdog.
7. Log only state, error, or feedback-warning changes.

An interval above 50 ms, clock rollback, read/write failure, Hall failure, or
watchdog service failure becomes a latched hardware/control error.

## State Machine

- `SAFE_START`: STOP until controls are released and safety inputs remain valid
  for 500 ms. A held boot button cannot start movement.
- `UNREFERENCED`: STOP after boot until a deliberate downward calibration
  establishes a common lower reference for all three axes.
- `IDLE`: STOP, waiting for a debounced hold-to-run request.
- `MOVING_UP` / `MOVING_DOWN`: independent axis output, Hall checks, soft start,
  synchronization trim, limits, and alignment timeout.
- `CALIBRATING_DOWN`: explicit 2 second CALIBRATE hold; each axis stops at its
  lower limit and Hall counters are zeroed only after all three arrive.
- `ERROR`: latched STOP. Hold CALIBRATE for 2 seconds and release with all inputs
  healthy to return to `SAFE_START`.

CALIBRATE release aborts calibration. UP plus DOWN, E-stop, limit conflict,
feedback faults, invalid transition, loop overrun, and hardware failures latch
`ERROR`.

## Motor Boundary

`motor_axis.c` validates individual commands and complete output vectors. It
checks state/error coherence, duty range, STOP de-energization, and direction
compatibility. `esp_gpio_hal_apply_outputs()` validates all three commands before
applying any axis. A later driver error triggers best-effort STOP on every axis.

The platform executes a host-tested BTS7960 operation sequence:

1. Enable LOW.
2. RPWM and LPWM duty zero.
3. Inactive direction duty zero.
4. Active direction duty set.
5. Enable HIGH.

After every command, the driver reads the GPIO enable level and both LEDC duty
registers back. Any mismatch forces STOP and propagates a platform fault to the
application. Domain dead time is 300 ms. The driver also forces STOP whenever
its applied direction changes, but does not replace domain timing or prove that
an external BTS7960 module obeyed its logic inputs.

## Hall Feedback

Each quadrature pair uses one ESP-IDF PCNT unit with a 1 us glitch filter,
accumulated count, and watch points at +/-30000. Counts are sampled in the
control task; no application ISR or unbounded callback exists.

During motion the domain checks:

- all three sources valid;
- count movement agrees with commanded direction;
- each enabled motor changes count before the stall timeout;
- global spread remains below the hard threshold;
- leading axes receive a configurable PWM reduction at the warning threshold.

Raw counts remain available for direction and stall checks. Each axis also has
an integer counts-per-travel scale that converts its count to micrometers before
skew comparison. Position becomes referenced only after successful lower
calibration. No preset or absolute automatic positioning is implemented.

## Extension Points

Future command sources submit semantic requests above `desk_app`; they cannot
receive motor/GPIO handles. Presets need a separate position-command service and
NVS adapter. WiFi/BLE need bounded queues into the application layer, with local
safety retaining final authority.
