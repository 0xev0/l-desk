# Commissioning Procedure

Do not skip directly to three loaded actuators. Stop at the first failed gate,
remove motor power, and diagnose it.

## 1. Desk Unpowered

- Confirm exact DevKit model/revision and every GPIO by continuity.
- Confirm no 5 V Hall or button signal reaches the ESP32.
- Verify NC limits and E-stop monitor read LOW healthy and HIGH when open.
- Verify the second E-stop contact drops the motor contactor without firmware.
- Confirm branch fuse values match cable and measured actuator current.
- Confirm protective earth, common signal ground, ferrules, strain relief, and
  separation of motor/signal wiring.
- Confirm all BTS enable inputs have physical pulldowns.

## 2. Logic Only

Power the DevKit from USB or the final 5 V regulator with 12 V motor branches
disconnected.

- Boot with no buttons: outputs remain disabled for at least 500 ms.
- Boot holding each button and combinations: no movement output is enabled.
- Open every limit and E-stop monitor wire in turn; inspect expected error/state.
- Verify CALIBRATE and error-clear long-hold/release behavior.
- Confirm watchdog reset behavior and brownout recovery return to `SAFE_START`.
- Flash a signed baseline image and confirm an unsigned OTA image is rejected.

## 3. Outputs Without Motors

Use LEDs/appropriate test loads or an oscilloscope at each BTS input.

- STOP: both PWM low and enable low.
- UP/DOWN: exactly one PWM active after enable sequencing.
- Reverse while held: immediate STOP and at least 300 ms before opposite PWM.
- Reset and failed initialization: all enables remain low.
- Repeat for axes A, B, and C.
- Scope RPWM/LPWM and prove they never overlap as active directions.

## 4. One Actuator, No Load

Physically power only one motor branch. Keep unused NC safety loops healthy. For
a temporary one-axis Hall test, the other motor branches remain disconnected;
do not install that temporary setup in the desk.

- Jog briefly at low PWM and confirm UP increases Hall count.
- Confirm DOWN decreases Hall count. Correct channel order/sign centrally.
- Measure Hall counts over a known travel in both directions several times.
- Enter the measured counts-per-travel value independently for all three axes;
  do not assume that nominally identical Hall sensors have identical scale.
- Measure no-load running, startup, and safely limited stall current.
- Verify upper/lower endstops stop the correct direction and allow retreat.
- Verify stall detection by disconnecting one Hall channel only under controlled
  no-load conditions.

Repeat for all three actuator/driver pairs.

## 5. Three Actuators, Unloaded Frame

- Install all guides and the rigid frame, without the final tabletop/load.
- Enter measured Hall scales, warning/stop distances, and conservative PWM in
  `board_config.h`, rebuild, and repeat output tests.
- Calibrate down while holding CALIBRATE; release must abort immediately.
- Confirm successful calibration zeroes all counters.
- Run short UP/DOWN strokes while measuring physical level at all columns.
- Tune soft-start, maximum PWM, synchronization trim, and count thresholds.
- Inject each Hall and limit disconnection one at a time.
- Confirm one endstop arrival stops that axis and times out within 750 ms if the
  other axes do not arrive.

## 6. Progressive Load

Test frame-only, then 25%, 50%, 75%, and expected working load.

- Keep skew below 5 mm throughout travel; warning should occur by 3 mm.
- Record current and supply voltage during simultaneous start and reversal.
- Confirm no ESP32 brownout or contactor chatter.
- Record BTS, connector, fuse holder, wire, regulator, and PSU temperatures.
- Respect the actuator's specified duty cycle, expected near 10% if applicable.
- Inspect binding, noise, loose terminals, and frame deformation after each step.

## 7. Release Tests

- Physical E-stop at idle, UP, DOWN, and calibration.
- Open every limit and Hall wire during controlled motion.
- Repeated reset, brownout, PSU interruption, and at least 100 power cycles.
- At least 8 hours powered soak plus repeated movement within duty limits.
- Record minimum free heap and prove it stabilizes.
- Exercise local, hybrid, and mobile-only modes with buttons connected and
  disconnected as intended for the final installation.
- Test phone blur, screen lock, WiFi loss, AP restart, and WebSocket loss during
  each hold action; motion must stop within the 250 ms lease.
- Capture and move toward all three presets from above and below under load.
- Install a valid signed OTA, verify inactive-slot boot and confirmation, then
  install a deliberately failing test image and verify rollback.
- Interrupt OTA upload and power at controlled stages; the previous app must
  remain bootable and motor outputs inactive.
- Re-torque allowed terminals and inspect for thermal discoloration.
- Review logs and every commissioning result before creating a release tag.

The release is blocked until all tests pass with the final actuator, driver,
PSU, wiring, enclosure, mechanical frame, firmware commit, and tuning values.
