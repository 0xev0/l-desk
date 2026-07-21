# Troubleshooting

Always remove motor power before changing wiring. Use the physical E-stop, not a
software command, when motion is unexpected.

| Symptom | Likely causes | Checks |
| --- | --- | --- |
| Stays in `SAFE_START` | Held control, invalid Hall, safety input not released | Monitor raw wiring, release controls for 500 ms |
| `HARDWARE_FAULT` at boot | GPIO/Hall/PCNT/watchdog initialization failed | Read first ESP-IDF error log, verify pinout and build target |
| `LIMIT_CONFLICT` | Both limits open on one axis or common wire broken | Continuity-test both NC loops with power removed |
| `FEEDBACK_MISSING` | Hall read invalid/platform fault | Verify 3.3 V levels, ground, connector, PCNT pins |
| `FEEDBACK_DIRECTION` | Hall sign/channel order or motor polarity wrong | Jog unloaded, ensure UP count positive on every axis |
| `FEEDBACK_STALL` | Motor not moving, Hall disconnected, PWM too low, binding | Remove load, inspect pulses/current/mechanics |
| `FEEDBACK_SKEW` | Wrong counts/mm, unequal motion, slipping/binding | Measure each axis travel and Hall scale, inspect frame |
| `ALIGNMENT_TIMEOUT` | Other axes did not reach matching endstop in 750 ms | Inspect limit placement, stroke equality, mechanical level |
| `CONTROL_LOOP_OVERRUN` | Task blocked over 50 ms or clock anomaly | Inspect logs/tasks; keep networking outside control path |
| Driver heats rapidly | Clone module underrated, poor terminal/heatsink, overload | Stop; measure current and temperature, inspect module revision |
| ESP32 resets on start | PSU/regulator sag, ground layout, motor noise | Scope 12 V/5 V/3.3 V, separate wiring, inspect suppression |
| Native USB unavailable | GPIO19/20 assigned to Hall | Use DevKit USB-to-UART port |

## Error Clear

Fix the cause first. Release UP/DOWN and E-stop, ensure there is no limit
conflict and Hall inputs are valid, hold CALIBRATE for 2 seconds, then release.
The desk returns to `SAFE_START`; it will not move until controls remain released
for another 500 ms and a new hold-to-run command is made.

If the error immediately returns, do not repeatedly clear it. Remove motor power
and diagnose the corresponding input, feedback, timing, or hardware path.
