# Security Audit

Audit date: 2026-07-24

## Result

No known critical or high-severity software finding remains open in the
reviewed source. This statement is not a certification and does not cover
uncommissioned hardware, future dependencies, undiscovered vulnerabilities,
key custody failures, or physical fault injection.

The control path is statically allocated, bounded, host-tested, and isolated
from ESP-IDF by HAL/application boundaries. Remote input remains leased,
hold-to-run, single-owner, and subordinate to the same state machine as local
input.

## Closed Findings

| Finding | Resolution |
| --- | --- |
| Plain HTTP/WebSocket exposed token and commands | HTTPS/WSS only, exact HTTPS origin, secure cookies, CSP, and no plaintext fallback |
| One image could share credentials across devices | Release build injects a unique per-device secret/certificate and removes generated source |
| Provisioning/admin secrets appeared on UART | All secret logging removed; offline credential record required |
| Physical flash exposed firmware, WiFi, and tokens | Secure Boot v2, release-mode flash encryption, encrypted NVS, and restricted ROM download profile |
| Signed downgrade remained possible | Bootloader anti-rollback and secure version enabled |
| Secure bootloader exceeded old partition offset | Partition table moved to `0x10000`; signed bootloader and app size gates pass |
| NVS encryption selected an invalid HMAC eFuse block | NVS keys use flash-encryption protection and dedicated encrypted key partition |
| Login modal could trap mobile users | Explicit close, cancel, Escape, and nonmodal retry path |
| Connection state was ambiguous | HTTPS, API/session, WSS lease, and freshness are displayed separately |
| Buttonless error recovery required reboot/hidden gesture | Authenticated semantic clear request reuses domain safety validation and returns to `SAFE_START` |
| Blocking or stale status polling could multiply | Fetch timeout, non-overlapping poll, and one rescheduled timer |

## Reviewed Controls

- bounded C parsing, fixed request/body/session capacities, and login throttling;
- constant-time administration secret/session comparison;
- `Secure`, `HttpOnly`, `SameSite=Strict` cookies and no-store responses;
- exact same-origin enforcement for every mutation and WSS handshake;
- one WSS control owner, random lease ID, monotonic sequence, 250 ms expiry;
- STOP on pointer release/cancel, page hide, blur, timeout, error, and close;
- signed OTA, maintenance inhibit, exact upload length, A/B rollback;
- unique release credentials, secret file permissions, and ignored key patterns;
- flash/NVS/coredump/diagnostic protection in the production partition profile;
- no project-owned control-path heap or unbounded domain work;
- error-output invariants and mutually exclusive motor directions.

## Residual Boundaries

These are not firmware defects that can be fixed away; they are release
boundaries that must remain controlled.

### Physical Destruction and Denial of Service

Secure Boot prevents untrusted execution and flash encryption protects
confidentiality, but someone with enclosure/eFuse/power access can still erase
flash, damage wiring, hold E-stop, fault the supply, or destroy the controller.
Mitigation is restricted physical access, enclosure, recovery custody, and a
physical E-stop that cuts motor power independently.

### TLS and LAN Ownership

The device certificate is supplied by the deployer. A wrong private CA, DNS,
reverse-proxy, renewal, or phone trust configuration can make the console
unavailable or train users to ignore certificate warnings. Commission either a
private-CA direct path or HTTPS-to-HTTPS reverse proxy as documented. Never
click through a certificate mismatch for motor control.

### Hardware and Mechanical Evidence

Automated builds cannot prove BTS7960 clone ratings, GPIO voltages, contactor
interruption, Hall integrity, PSU transients, EMC, thermal margin, frame
rigidity, or pinch-zone safety. Every commissioning gate remains mandatory.

### Upstream and Future Code

ESP-IDF, mbedTLS, WiFi firmware, toolchains, and future integrations can receive
security updates or new findings. CI, action dependency updates, signed
releases, and private reporting reduce response time but do not remove that
maintenance responsibility.

## Release Decision

The firmware is software-build-ready. It is not desk-production-ready until:

- exact hardware and wiring pass commissioning;
- signing/TLS keys and device records have protected backups;
- private CA or reverse-proxy trust works without browser warnings;
- first-boot eFuse state is captured and verified;
- physical E-stop interrupts motor power with MCU unresponsive;
- fault injection, power-cycle, OTA rollback, thermal, loaded skew, and soak
  tests pass.
