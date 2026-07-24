# Releasing

Only maintainers may create releases. A tagged release represents a specific
source revision and documented hardware evidence; it is not a certification.

## Versioning

- Use Semantic Versioning tags such as `v0.1.0`.
- Keep all commits conventional so release notes can be audited.
- Update `CHANGELOG.md` before tagging.
- Do not reuse or move a published tag.
- Increment the ESP anti-rollback secure version only when intentionally
  revoking every older signed firmware image.

## Required Gates

1. CI passes host sanitizers, coverage, architecture checks, and the ESP-IDF
   secure-profile build.
2. The exact source revision passes `tools/build_release.sh` with protected
   production keys.
3. Bootloader and application signatures verify and `SHA256SUMS` is archived.
4. Every applicable item in `docs/commissioning.md` passes on the final desk.
5. E-stop physical power interruption, loaded skew, thermal, power-loss, OTA
   rollback, and soak evidence is archived.
6. `docs/security-audit.md`, `SECURITY.md`, and all deployment instructions are
   current.

## Device Artifacts

Do not attach a universal production firmware binary to a public release.
Administration tokens and TLS credentials are unique per desk, and production
images must be generated per device.

A public GitHub release may contain source and non-secret documentation.
Per-device signed firmware, TLS material, generated credentials, eFuse
summaries, and commissioning records remain in protected operator storage.

## Publication

1. Create an annotated signed tag from a clean worktree.
2. Push the tag only after all gates pass.
3. Create GitHub release notes from the changelog and conventional commits.
4. Link the exact supported hardware revision and known limitations.
5. Preserve the previous known-good signed image for rollback where the secure
   version policy still permits it.
