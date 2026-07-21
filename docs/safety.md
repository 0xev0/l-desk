# Safety Model

## Firmware Guarantees

- Startup/reset enters `SAFE_START`; controls must remain released for 500 ms.
- STOP is represented by zero duty, no direction, and enable false.
- Complete output coherence is checked before any axis is applied.
- Emergency, hardware, loop, feedback, and invalid-input errors stop all axes.
- Matching NC endstops stop an axis immediately and latch it for that request.
- Opposite movement away from one active endstop is allowed.
- Upper plus lower active on one axis is a hardware conflict.
- Direction reversal stops immediately and waits 300 ms.
- UP plus DOWN is a latched error.
- Calibration requires CALIBRATE held for 2 seconds and aborts on release.
- Each remaining axis receives at most 750 ms after another reaches an endstop.
- Missing Hall, wrong direction, 1 second stall, or excessive skew stops all.
- A control-loop interval above 50 ms becomes a latched error.
- Error clear requires CALIBRATE held 2 seconds and then released with healthy
  inputs; it returns to `SAFE_START` and never resumes movement automatically.

## Input Filtering

Limits and E-stop activate on the first sampled HIGH. Their inactive state must
remain stable for 50 ms before clearing. Button starts are debounced for 50 ms;
button release and all stop paths are immediate.

An open NC wire is intentionally indistinguishable from an active safety input.
This prevents motion farther toward the affected end but does not identify the
physical cause.

## Hall Synchronization Limits

Incremental Hall counts require a successful lower calibration after every lost
position/reset. The current release does not move to automatic positions. PWM
trim reduces the leading axis; it is not a PID and cannot compensate for a bent
frame, overloaded column, slipping mechanics, or sensor pulses that look valid
but are physically wrong.

The 3 mm warning and 5 mm hard-stop goals are acceptance criteria. Firmware uses
raw count thresholds until measured pulses/mm are written into board tuning.

## Outside Firmware Control

Firmware cannot guarantee safety if:

- the contactor welds or the E-stop is not independently wired;
- a BTS7960 fails short;
- motor or mains wiring is undersized, loose, or improperly protected;
- the frame racks, binds, tips, or uses actuators as lateral guides;
- mechanical hard stops are absent;
- Hall wiring creates plausible but false counts;
- the selected drivers, PSU, connectors, or enclosure overheat.

Use a DC-rated physical contactor, branch fuses, proper earth/grounding,
strain relief, guarded pinch zones, and a rigid guided structure. Keep a hand on
the E-stop during first motion and never commission under the final tabletop.

## Release Gate

"Firmware build ready" means tests and compilation pass. "Desk release ready"
requires every item in `commissioning.md`, including physical E-stop tests,
oscilloscope verification, loaded skew, thermal tests, repeated power cycles,
and an 8 hour soak. No release tag should be made before that evidence exists.
