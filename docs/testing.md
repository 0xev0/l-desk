# Testing

Domain tests run on the host without ESP-IDF hardware. The controller receives normalized inputs, so tests can exercise safety behavior deterministically without GPIO.

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
- Independent axis stop and alignment timeout
- Calibration-down behavior
- Error state and clear-error behavior
- Edge cases and invalid inputs

## Safety-Critical Cases

Tests must prove that no state commands unsafe motor output, emergency stop always stops motion, active matching limits block movement, impossible limit combinations enter ERROR, and timeout behavior is deterministic.
