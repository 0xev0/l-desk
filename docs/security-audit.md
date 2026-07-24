# Security Audit

Audit date: 2026-07-24

## Executive Summary

No open critical or high-severity software vulnerability was found in the
reviewed Phase 1 through P5A firmware. The safety control path is statically
allocated, host-tested, isolated from ESP-IDF by the HAL, and remains
authoritative over every local or remote request.

The remaining security findings are accepted homelab deployment constraints,
not hidden implementation defects. They must be respected during installation.
The desk is not physically release-ready until the final hardware completes
the commissioning and soak gates.

## Scope

The review covered:

- domain, application, HAL, ESP-IDF platform, networking, OTA, and NVS code;
- the embedded HTML, CSS, and JavaScript mobile controller;
- unsafe C APIs, dynamic allocation, unbounded work, output coherency, and
  platform dependency boundaries;
- authentication, sessions, origin checks, WebSocket leases, provisioning,
  signed updates, rollback, and secret handling;
- host tests with ASan/UBSan, coverage, strict formatting, and ESP-IDF 6.0
  release compilation.

## Open Findings

### SEC-001: Plaintext Control on the Local LAN

Severity: Medium

Location: `docs/networking.md:36`, `main/network/esp_http_api.c:48`

Evidence: the controller serves HTTP and `ws://`. It applies CSP, frame denial,
MIME sniffing denial, no-store responses, exact same-origin checks, and
authenticated `HttpOnly` sessions, but it does not provide transport
encryption.

Impact: a hostile client with the ability to observe traffic on the same LAN
could steal an administration token or session and submit authenticated
hold-to-run requests.

Required mitigation: keep the desk on a trusted WPA2/WPA3 LAN or isolated IoT
VLAN, never port-forward it, and do not expose it to the Internet. A future TLS
transport may reduce this risk, but it must retain the existing 250 ms lease
expiry and safety-state boundary.

### SEC-002: Physical Flash Access Is Trusted

Severity: Medium

Location: `docs/updates.md:5`

Evidence: OTA images require RSA-3072 signatures, while hardware Secure Boot,
flash encryption, and NVS encryption are intentionally disabled.

Impact: someone with physical UART/flash access can replace firmware or recover
stored WiFi credentials and device secrets.

Required mitigation: keep UART and electronics inside the enclosure. Treat
physical access as administrative access. Enable eFuse-backed controls only
after a tested recovery process exists; doing so prematurely can make a
homelab controller unrecoverable.

### SEC-003: Provisioning Secrets Appear on UART

Severity: Low

Location: `main/network/esp_network_service.c:47`,
`main/network/esp_wifi_service.c:13`

Evidence: the generated SoftAP password and 128-bit administration token are
printed when provisioning is active. This is the deliberate bootstrap channel
for a headless device.

Impact: retained serial logs or an exposed UART reveal credentials.

Required mitigation: provision in a private environment, do not retain shared
terminal logs, and enclose the UART after commissioning. No fixed or
repository-owned password exists.

### SEC-004: Signed Downgrade Is Possible

Severity: Low

Location: `docs/updates.md:16`

Evidence: anti-rollback eFuses are not enabled, so an older image signed by the
trusted release key remains acceptable.

Impact: an administrator or attacker holding both an authenticated session and
a valid old signed image can restore a version containing older defects.

Required mitigation: protect the signing key, control release artifacts, and
verify the displayed version after updates. This tradeoff deliberately
preserves serial recovery.

## Verified Controls

- No project-owned `malloc`, `calloc`, `realloc`, `free`, or unsafe string-copy
  call exists in the control code.
- Domain and HAL sources contain no ESP-IDF GPIO, PCNT, LEDC, FreeRTOS, or task
  dependency. `desk_app_runtime.c` is the explicit ESP-IDF composition boundary.
- Motor vectors are validated before application; incoherent direction,
  enable, duty, or readback causes a stopped hardware-fault state.
- Remote motion uses an exclusive lease, monotonic sequence checks, local
  preemption, and a 250 ms expiry. Network loss cannot leave motion latched.
- Browser code has no HTML injection, dynamic-code execution, browser storage,
  third-party script, or cross-window messaging sink.
- Mutating HTTP and WebSocket requests require authentication and exact origin;
  login attempts, sessions, frame sizes, bodies, timeouts, and queues are
  bounded.
- OTA requires a signed image, maintenance inhibit, confirmed STOP output,
  inactive-slot writes, boot health confirmation, and rollback support.
- C and web contract tests pass under sanitizers; ESP-IDF builds with warnings
  as errors, assertions, performance optimization, watchdogs, and strong stack
  checking.

## Physical Release Blockers

Software tests cannot validate voltage levels, Hall signal compatibility,
BTS7960 clone behavior, contactor interruption, fuse coordination, motor
current, conducted noise, RF performance, thermal margins, mechanical skew, or
real power-loss timing. Every step in `commissioning.md` remains mandatory.

No release tag should be created until those records, including the eight-hour
soak and repeated E-stop/brownout/OTA rollback tests, pass on the exact final
assembly.
