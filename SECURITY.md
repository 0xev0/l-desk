# Security Policy

## Supported Versions

Security fixes are provided for the latest tagged release and the current
default branch. Development snapshots and older releases may receive fixes only
when the same change applies cleanly.

## Reporting a Vulnerability

Do not open a public issue for:

- authentication or session bypass;
- remotely triggerable movement or safety-boundary bypass;
- leaked keys, tokens, certificates, or WiFi credentials;
- unsigned or downgraded firmware acceptance;
- memory corruption, denial of service, or persistent crash;
- a way to activate both motor directions or bypass E-stop/limit behavior.

Use GitHub's **Security > Report a vulnerability** private reporting flow.
Include the affected commit or release, hardware context, reproduction steps,
impact, and any suggested mitigation. Do not attach real credentials or private
keys.

You should receive an acknowledgement within seven days. The maintainer will
validate the report, coordinate a fix and disclosure, and credit reporters who
request attribution. No bounty program is offered.

## Deployment Boundary

This is a safety-conscious homelab controller, not a certified safety PLC.
Firmware security cannot compensate for an exposed mains supply, an incorrectly
wired E-stop, a welded contactor, a failed H-bridge, missing fuses, unsafe
mechanics, or unrestricted physical access.

Production-profile devices use HTTPS/WSS, unique per-device credentials, signed
images, Secure Boot v2, flash encryption, encrypted NVS, anti-rollback, A/B OTA,
and restricted ROM download mode. These protections must be commissioned with
the exact hardware and key custody process described in
[`docs/secure-deployment.md`](docs/secure-deployment.md).
