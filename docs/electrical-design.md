# Electrical Design

## Power Topology

```text
AC mains -> protected 12 V PSU
                    |
                    +-> logic fuse -> 12/5 V regulator -> ESP32 + BTS logic
                    |
                    +-> main motor fuse -> E-stop contactor -> 3 fused branches
                                                           -> BTS A/B/C -> motors
```

The logic branch stays powered when the E-stop drops the motor contactor so the
ESP32 can record the event and remain in `ERROR`. All 0 V logic and motor grounds
share a deliberate common reference. Protective earth follows PSU, enclosure,
and frame manufacturer requirements; it is not the same as signal ground.

Never route motor current through a breadboard, DevKit adapter, Dupont lead, or
small signal terminal. Separate motor, Hall, limits, and button harnesses.

## BTS7960 Per Axis

- `B+`/`B-`: fused 12 V motor branch and motor return.
- `M+`/`M-`: actuator motor leads; swap only after confirming direction.
- `VCC`: regulated 5 V logic if required by the module revision.
- `GND`: common logic ground.
- `RPWM`/`LPWM`: separate 20 kHz ESP32 LEDC outputs.
- `R_EN` and `L_EN`: tied together and driven by the axis enable GPIO.
- Enable needs a physical pulldown so reset/boot keeps the bridge disabled.

Confirm the exact module accepts ESP32 3.3 V HIGH on PWM and enable inputs.
Clone modules differ. Verify heatsinking, terminals, copper, and thermal rise
with the purchased revision; the advertised chip current is not the module's
guaranteed continuous current.

## NC Inputs

Each limit and the monitored E-stop contact closes to ground when healthy:

```text
LOW  = closed, healthy, inactive
HIGH = open, reached, E-stop active, or broken wire
```

The firmware enables internal pull-ups. For long cables, fit suitable external
pull-ups near the controller and validate EMC behavior. Limit/E-stop activation
is immediate; only release is filtered for 50 ms.

Use separate E-stop contacts: one in the contactor safety circuit and one GPIO
monitor contact. Firmware detection is supplementary and must not be required
to remove motor power. Fit coil suppression appropriate to the DC contactor.

## Hall Inputs

Six Hall channels connect to PCNT GPIOs. Open-collector outputs require pull-ups
to 3.3 V. A 5 V push-pull output must pass through a proper buffer. Keep Hall
cables away from motor leads, use twisted signal/ground pairs or shielding as
appropriate, and validate the 1 us digital glitch filter against real pulses.

## Startup and Failure

GPIO enables have pulldowns. Firmware initializes enable LOW and PWM zero before
any motor driver can be enabled. Any initialization, output, Hall, or watchdog
failure leaves requested outputs stopped. A physical power interruption must
also default the contactor open.
