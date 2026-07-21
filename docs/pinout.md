# ESP32-S3 Pinout

Target: `ESP32-S3-DevKitC-1U-N8R8`. Definitions are centralized in
`main/platform/board_config.h` and remain provisional until continuity and board
revision checks are complete.

| Function | GPIO | Semantics |
| --- | ---: | --- |
| Axis A RPWM / LPWM / EN | 4 / 5 / 6 | 20 kHz active-high PWM, active-high enable |
| Axis B RPWM / LPWM / EN | 7 / 8 / 9 | 20 kHz active-high PWM, active-high enable |
| Axis C RPWM / LPWM / EN | 10 / 11 / 12 | 20 kHz active-high PWM, active-high enable |
| Axis A upper / lower limit | 13 / 14 | NC, HIGH active/fault |
| Axis B upper / lower limit | 15 / 16 | NC, HIGH active/fault |
| Axis C upper / lower limit | 17 / 18 | NC, HIGH active/fault |
| Hall A channels A / B | 19 / 20 | Quadrature, 3.3 V only |
| Hall B channels A / B | 38 / 39 | Quadrature, 3.3 V only |
| Hall C channels A / B | 40 / 41 | Quadrature, 3.3 V only |
| UP / DOWN / CALIBRATE | 1 / 2 / 21 | Active low |
| Monitored E-stop | 47 | NC-to-ground healthy, HIGH active/fault |
| On-board RGB LED | 48 | Reserved, not currently driven |

## Constraints

- GPIO19/20 are ordinary GPIO here, so native USB is unavailable. Flash and
  monitor through the DevKit USB-to-UART connector.
- GPIO0, 3, 45, and 46 are avoided because of strapping/special behavior.
- GPIO35, 36, and 37 are avoided because N8R8 modules use them for octal PSRAM.
- Verify the exact DevKitC-1U board revision and header breakout before wiring.
- Hall sign macros must make every count increase during UP and decrease during
  DOWN. Reverse a channel pair or change only its centralized sign after bench
  verification.
- No 5 V signal may reach an ESP32 GPIO.

## Tuning Values

`board_config.h` also centralizes Hall warning/stop counts, Hall stall timeout,
start/max PWM, and synchronization trim. The checked-in 30/50 count thresholds
are conservative provisional values, not yet proven to equal 3/5 mm. Measure
pulses per millimeter on each selected actuator before loaded commissioning.
