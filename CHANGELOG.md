# Changelog

All notable changes will be recorded here. The project follows
[Semantic Versioning](https://semver.org/) for tagged releases and uses
[Conventional Commits](https://www.conventionalcommits.org/) between releases.

## Unreleased

### Added

- Three-axis motion controller with Hall synchronization and calibration.
- Local, hybrid, and mobile-only hold-to-run control.
- Three hold-to-run presets.
- Authenticated HTTPS/WSS browser controls and protected Wi-Fi setup.
- Signed A/B OTA, diagnostics, Secure Boot, flash encryption, and anti-rollback
  production profile.
- Host sanitizers, coverage gates, controller invariants, and ESP-IDF 6.0 CI.

### Security

- Per-device administration and TLS credentials.
- Exact-origin mutation and WebSocket validation.
- Fixed-capacity sessions, login throttling, replay-resistant control leases,
  and fail-stopped command expiry.

No production release has been tagged. Hardware commissioning remains
mandatory before `v1.0.0`.
