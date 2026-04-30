# Safety

This project controls a large moving desk. Firmware must fail safe, but firmware cannot compensate for unsafe mechanical or electrical construction.

## Assumptions

- The tabletop sits on a rigid steel frame.
- Actuators provide vertical force only.
- Lateral loads and racking forces are handled mechanically.
- Physical fuses and emergency stop hardware exist.
- Limit switches are normally closed and treated as fail-safe.
- Phase 1 has no position feedback or encoder synchronization.

## Firmware Protections

- No automatic movement on boot.
- Manual hold-to-run only.
- Emergency stop immediately commands both motors stopped.
- Emergency and limit activation is passed through immediately by the app input filter.
- Emergency and limit release is debounced before motion can be re-enabled.
- Both UP and DOWN pressed stops both motors.
- Manual button presses must remain stable before motion starts.
- Calibration is rejected if UP or DOWN is held.
- Matching active limit switch blocks motion farther into that limit.
- Direction reversal enforces dead time.
- Direction reversal stop/dead-time is not delayed by button debounce.
- One side reaching a limit first starts a short alignment window.
- Alignment timeout enters ERROR.
- Calibration timeout enters ERROR with `DESK_ERROR_CALIBRATION_TIMEOUT`.
- ERROR inhibits motion until `desk_controller_request_clear_error()` or reboot.
- A side that reaches a matching limit is latched stopped until the current motion request ends or calibration completes.
- BTS7960 platform output is checked for command validity and coherent RPWM/LPWM/enable state before GPIO is updated.

## Known Limitations

- The firmware cannot detect desk tilt without sensors.
- The firmware cannot prove both actuators are synchronized during mid-stroke motion.
- A wiring fault on a limit switch is treated as an active limit or hardware fault, not diagnosed precisely.
- Phase 1 has no persistent fault log; errors are visible through controller state/log output only.
- Physical emergency stop, fuses, cable sizing, grounding, and mechanical stops remain mandatory.
