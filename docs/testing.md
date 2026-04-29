# Testing

Domain tests run on the host without ESP-IDF hardware. The controller receives normalized inputs, so tests can exercise safety behavior deterministically without GPIO.

`tests/fake_gpio.*` and `tests/fake_time.*` provide host-testable HAL fakes for future app-level tests. Current tests focus on the domain state machine and motor output invariants.

## Run Tests

```sh
cmake -S tests -B tests/build
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

## Test Categories

- Motor output safety
- Manual hold-to-run movement
- NC limit switch handling
- Emergency stop behavior
- Direction-change dead time
- Manual button debounce
- Independent axis stop and alignment timeout
- Calibration-down behavior
- Calibration invalid transition behavior
- Error state and clear-error behavior
- Edge cases and invalid inputs
- Limit switch bounce latching
- Null arguments and default configuration
- Large elapsed time values

## Safety-Critical Cases

Tests must prove that no state commands unsafe motor output, emergency stop always stops motion, active matching limits block movement, impossible limit combinations enter ERROR, and timeout behavior is deterministic.
