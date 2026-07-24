## Summary

<!-- What changes, and why is it needed? -->

## Safety and Security

- [ ] All error and disconnect paths stop every affected axis.
- [ ] No GPIO, PWM, PCNT, NVS, or ESP-IDF dependency entered the domain layer.
- [ ] No secret, credential, certificate key, generated binary, or private log is included.
- [ ] Remote behavior still passes through the command arbiter and domain safety state machine.
- [ ] Hardware, flash layout, OTA, or compatibility impact is documented below.

## Verification

- [ ] Host tests pass with ASan and UBSan.
- [ ] Relevant failure, boundary, recovery, and happy-path tests were added or updated.
- [ ] ESP-IDF 6.0 firmware builds with warnings as errors.
- [ ] Documentation is updated.
- [ ] Physical testing performed, or the missing hardware evidence is stated clearly.

## Hardware Evidence

<!-- Instruments, load, temperatures, timing, screenshots, or "not physically tested". -->

## Breaking Changes

<!-- API, pinout, persisted data, partition table, signing keys, commissioning, or none. -->
