# Contributing

Thank you for helping improve L-Desk. This firmware controls heavy moving
hardware, so safety regressions receive the same scrutiny as functional bugs.

## Before Opening a Pull Request

1. Open an issue for behavior changes, pin changes, new hardware, or protocol
   changes.
2. Keep the change inside the documented scope and architecture.
3. Add or update tests before or alongside implementation.
4. Run the host quality gates and an ESP-IDF build.
5. Update the affected documentation.

Never include WiFi credentials, certificates, private keys, administration
tokens, serial logs containing secrets, or generated release binaries.

## Development Setup

Requirements:

- CMake 3.22 or newer
- Clang or GCC with C11 support
- Python 3
- Node.js for JavaScript syntax checks
- ESP-IDF 6.0 for firmware builds

Run host tests:

```sh
cmake -S tests -B build-host -DDESK_ENABLE_SANITIZERS=ON
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
```

Format changed C files with the repository `.clang-format`. Build firmware
through the documented credential-aware flow in
[`docs/secure-deployment.md`](docs/secure-deployment.md).

## Engineering Rules

- Domain code remains pure C without ESP-IDF or FreeRTOS.
- GPIO, PWM, PCNT, NVS, network, and time dependencies stay behind HAL/platform
  boundaries.
- Control work remains bounded, statically allocated, and non-blocking.
- Every error path must produce a complete three-axis STOP vector.
- Remote features submit semantic requests and never touch motor GPIO.
- New dependencies require a concrete maintenance and security justification.
- Do not weaken a timeout, assertion, or test to accommodate faulty hardware.

## Tests

Changes must include relevant normal, boundary, failure, and recovery cases.
Safety behavior needs deterministic tests proving STOP, latch, and release
semantics. Hardware-related changes must state what remains unverified without
physical equipment.

## Commits and Pull Requests

Use conventional commits such as:

```text
fix: stop remote motion when control socket closes
feat: expose bounded diagnostic counters
test: cover all three-axis endstop arrival orders
docs: clarify secure first-flash procedure
```

Keep commits reviewable and independently buildable when practical. Complete
the pull request checklist and call out any safety, security, hardware, flash
layout, or compatibility impact.

By contributing, you agree that your contribution is licensed under the
repository's [PolyForm Noncommercial License 1.0.0](LICENSE).
