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
- Both UP and DOWN pressed stops both motors.
- Matching active limit switch blocks motion farther into that limit.
- Direction reversal enforces dead time.
- One side reaching a limit first starts a short alignment window.
- Alignment timeout enters ERROR.
- Calibration timeout enters ERROR.
- ERROR inhibits motion until explicit clear or reboot.

## Known Limitations

- The firmware cannot detect desk tilt without sensors.
- The firmware cannot prove both actuators are synchronized during mid-stroke motion.
- A wiring fault on a limit switch is treated as an active limit or hardware fault, not diagnosed precisely.
- Physical emergency stop, fuses, cable sizing, grounding, and mechanical stops remain mandatory.
