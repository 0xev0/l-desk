# Motorized Desk Firmware

ESP-IDF firmware for a custom two-actuator L-shaped standing desk controller.

Phase 1 is intentionally conservative: manual hold-to-run movement, calibration downward to the lower endstops, fail-safe normally-closed limit switch handling, emergency stop handling, direction-change dead time, and alignment timeout protection.

## Hardware Summary

- ESP32-S3 DevKitC-1
- Two 12 V linear actuators, one per powered lifting column
- Two BTS7960 H-bridge motor drivers
- Four normally-closed limit switches
- UP, DOWN, optional CALIBRATE buttons
- Emergency stop / safety input
- 12 V 20/30 A PSU, general fuse, and individual actuator fuses
- 12 V to 5 V regulator for ESP32 power

The actuators provide vertical force only. The desk frame and mechanical guides must handle lateral loads and racking forces.

## Phase 1 Scope

Implemented:

- Manual hold-to-run UP and DOWN
- Independent left/right motor outputs
- NC limit switch semantics
- Emergency stop detection
- Both-buttons-pressed safety stop
- Manual button debounce before motion starts
- Direction reversal dead time
- Alignment timeout when one side reaches an endstop first
- Calibration-down routine
- Calibration start rejected if UP or DOWN is held
- Host-testable domain logic
- ESP-IDF GPIO platform HAL for the ESP32-S3 target

Not implemented:

- WiFi, Bluetooth, web UI, Home Assistant, display, OTA
- Height memory or automatic positioning
- PID, encoders, closed-loop synchronization
- Persistent storage

## Build

Activate ESP-IDF if needed:

```sh
source /Users/4rkh4m/.espressif/tools/activate_idf_v6.0.sh
```

Set the target and build:

```sh
idf.py -B build_s3 set-target esp32s3
idf.py -B build_s3 build
```

## Flash

```sh
idf.py -p /dev/tty.usbmodemXXXX flash monitor
```

Replace the port with the connected ESP32-S3 serial device.

## Host Tests

Host-side tests use CMake/CTest and a normal C compiler:

```sh
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

The domain tests do not require ESP-IDF, FreeRTOS, GPIO hardware, or an attached desk.

## Safety Disclaimer

This firmware controls moving hardware capable of injury and property damage. Firmware is only one safety layer. Use correct mechanical design, physical emergency stop, fuses, cable sizing, grounding, actuator mounting, lateral guides, and mechanical hard stops.

When uncertain, the firmware must stop motion.
