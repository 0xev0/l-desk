# Phase Roadmap

## P1: Three-Axis Safety Controller

Status: firmware implemented and host/build verified; physical commissioning
pending.

- Three independent BTS7960 outputs
- Six NC limits, E-stop, manual UP/DOWN, and downward calibration
- `SAFE_START`, dead time, alignment timeout, watchdog, and latched errors
- HAL isolation, fake platform, sanitizers, and safety property tests

## P2: Hall Feedback and Synchronization

Status: firmware implemented in the first hardware release; actuator-specific
tuning and loaded validation pending.

- Three quadrature PCNT counters with accumulated counts and glitch filters
- Lower calibration zero, soft-start, leading-axis PWM trim
- Missing, wrong-direction, stalled, and excessive-skew faults
- No automatic height positioning

P1/P2 become release-ready together only after final hardware commissioning.

## P2.5: Control-Source Foundation

Status: implemented and host/build verified; HTTP/WebSocket transport pending.

- Local-only, hybrid, and mobile-only operating modes
- Exclusive remote lease with sequence anti-replay and 250 ms expiry
- Local preemption revokes the remote session
- Bounded static cross-core request queue
- Remote high-level calibration/error-clear gesture for buttonless operation

## P3: Local Presets

Start only after P1/P2 pass sustained loaded use.

- M1/M2/M3 local panel
- NVS records with version, CRC, bounds, and atomic replacement
- Position moves remain hold-to-run and always subordinate to local safety
- No direct motor commands or unattended motion

## P4: Diagnostics and Updates

- Bounded last-error/reset history
- A/B app partitions, rollback, and recovery procedure
- Signed firmware after update/recovery behavior is mature
- Secure Boot only after a tested homelab recovery plan; do not burn eFuses early

## P5: WiFi and Bluetooth

- Implemented foundation: validated NVS credentials, WiFi STA, first-boot
  protected SoftAP, and recovery SoftAP after bounded connection failures
- In progress: authenticated local HTTP/WebSocket API and installable mobile
  web UI
- BLE provisioning and local configuration
- WiFi STA plus MQTT/TLS telemetry for Home Assistant
- Separate network tasks and bounded queues into the application layer
- Remote STOP allowed; movement disabled by default
- Temporary local CALIBRATE authorization required for remote preset movement
- 250 ms command heartbeat; loss stops motion
- No remote calibration, error clear, or raw motor control
- Network failure must never affect local manual operation

These phases use the same ESP32-S3-1U and antenna. They do not justify adding a
second controller, custom PCB, display, or cloud dependency now.
