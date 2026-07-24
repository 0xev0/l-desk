# Phase Roadmap

## P1: Three-Axis Motion Controller

Status: firmware implemented and host/build verified; physical commissioning
pending.

- Three independent BTS7960 outputs
- Six NC limits, E-stop, manual UP/DOWN, and downward calibration
- `SAFE_START`, dead time, alignment timeout, watchdog, and latched errors
- HAL isolation, fake platform, sanitizers, and controller invariant tests

## P2: Hall Feedback and Synchronization

Status: firmware implemented in the first hardware release; actuator-specific
tuning and loaded validation pending.

- Three quadrature PCNT counters with accumulated counts and glitch filters
- Lower calibration zero, soft-start, leading-axis PWM trim
- Missing, wrong-direction, stalled, and excessive-skew faults
- No automatic height positioning

P1/P2 become release-ready together only after final hardware commissioning.

## P2.5: Control-Source Foundation

Status: implemented and host/build verified.

- Local-only, hybrid, and mobile-only operating modes
- Exclusive remote lease with sequence anti-replay and 250 ms expiry
- Local preemption revokes the remote session
- Bounded static cross-core request queue
- Remote high-level calibration and controller-validated fault reset

## P3: Presets

Status: mobile presets implemented and host/build verified; loaded target
accuracy pending.

- M1/M2/M3 capture only while idle, referenced, level, and feedback-valid
- NVS record with explicit byte order, version, CRC, bounds, and one-blob commit
- Position moves remain hold-to-run and subordinate to all P1/P2 safety checks
- Target crossing latches STOP for the gesture; no hunting or unattended motion
- Optional physical M buttons are not fitted in the minimum hardware

## P4: Diagnostics and Updates

Status: firmware implemented and build verified; power-loss/rollback tests on
the final DevKit are pending.

- Bounded CRC-protected reset/error history in a dedicated NVS partition
- Runtime heap, stack high-water, and dropped-log diagnostics
- A/B app partitions, rollback metadata, coredump partition, and recovery docs
- RSA Secure Boot v2 and signed application verification
- Release-mode flash encryption, encrypted NVS, and anti-rollback
- Motion inhibit before update and held-control release interlock after failure
- Pending image confirmation requires stopped control and network startup
- Restricted ROM download mode retains a controlled recovery path

## P5A: Wi-Fi and Browser Control

Status: implemented and host/build verified; RF and phone testing pending.

- Validated NVS credentials, Wi-Fi STA, and a password-protected setup SoftAP
- Authenticated local HTTPS/WSS API and responsive browser controls
- Buttons, buttons + phone, and buttonless phone modes
- Exclusive lease, anti-replay sequence, 100 ms heartbeat, 250 ms expiry
- Remote calibration is hold-to-run; fault reset returns to `SAFE_START`
- No raw motor, GPIO, per-axis, or unattended motion command
- Network failure resolves phone input to STOP and leaves button control running

## P5B: Optional Integrations

Status: intentionally deferred beyond the one-month release.

- BLE is not needed for provisioning because protected SoftAP works on every
  target phone without a native app.
- MQTT/TLS and Home Assistant require a concrete broker/CA/topic deployment.
- Movement over MQTT remains disabled by default; telemetry and STOP come first.
- Any later transport uses immutable status snapshots and the existing bounded
  lease queue. It cannot touch GPIO or weaken local/E-stop priority.

These phases use the same ESP32-S3-1U and antenna. They do not justify adding a
second controller, custom PCB, display, native mobile app, or cloud dependency.
