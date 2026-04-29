# Pinout

The pin mapping is provisional and centralized in the platform configuration. Do not hardcode GPIO numbers outside the board/platform layer.

| Signal | GPIO | Electrical Semantics |
| --- | ---: | --- |
| Left RPWM | 4 | Output active high |
| Left LPWM | 5 | Output active high |
| Left enable | 15 | Output active high |
| Right RPWM | 6 | Output active high |
| Right LPWM | 7 | Output active high |
| Right enable | 8 | Output active high |
| Left upper limit | 16 | NC input, HIGH means active/fault |
| Left lower limit | 17 | NC input, HIGH means active/fault |
| Right upper limit | 18 | NC input, HIGH means active/fault |
| Right lower limit | 19 | NC input, HIGH means active/fault |
| UP button | 10 | Active low |
| DOWN button | 11 | Active low |
| CALIBRATE button | 12 | Active low |
| Emergency stop | 13 | Active high by default |

If future analog sensors are added, verify ESP32-S3 ADC-capable pins and boot strap constraints before assigning pins.
