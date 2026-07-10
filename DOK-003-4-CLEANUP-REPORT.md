# DOK-003.4 cleanup report

## Removed or consolidated

- `docs/firmware/abrp-time.md`
- `docs/firmware/can-decoder.md`
- `docs/firmware/lilygo-abrp-wifi.md`
- `docs/firmware/lilygo-can-decoder-mqtt-wifi.md`
- `docs/firmware/lilygo-can-sn65hvd230.md`
- `docs/firmware/lilygo-full-lewisxhe-a76xx-transport.md`
- `docs/firmware/lilygo-integration-cleanup.md`
- `docs/firmware/lilygo-lte-at-stack-v2.md`
- `docs/firmware/lilygo-lte-cipopen-urc-fix.md`
- `docs/firmware/lilygo-lte-ciprxget-receive-fix.md`
- `docs/firmware/lilygo-lte-debug-and-backup-fix.md`
- `docs/firmware/lilygo-lte-mqtt-trace.md`
- `docs/firmware/lilygo-lte-rx-debug.md`
- `docs/firmware/lilygo-lte-stack-v3-clean.md`
- `docs/firmware/lilygo-lte-tcp-missing-functions.md`
- `docs/firmware/lilygo-lte-tcp-test-endpoint.md`
- `docs/firmware/lilygo-lte-urc-rx-buffer.md`
- `docs/firmware/lilygo-modem-boot-recovery.md`
- `docs/firmware/lilygo-mqtt-debug.md`
- `docs/firmware/lilygo-mqtt-gps-diagnostics.md`
- `docs/firmware/lilygo-mqtt-lte-transport.md`
- `docs/firmware/lilygo-network-failover.md`
- `docs/firmware/lilygo-tinygsm-a76xx-transport.md`
- `docs/firmware/lilygo-tinygsm-lte-client.md`
- `docs/firmware/mqtt-diagnostics.md`
- `docs/firmware/no-firmware-change-v1.0.1.md`
- `docs/firmware/optional-services-and-config.md`
- `docs/firmware/system-health.md`
- `docs/firmware/v1.0.3-hotfix.md`
- `docs/firmware/mqtt-topics.md`
- `docs/ARCHITECTURE.md`
- `docs/DASHBOARD.md`
- `docs/firmware.md`
- `docs/developer.md`
- `docs/user-guide/`
- `docs/release/`
- `docs/release-notes/`
- `docs/hardware/esp32.md`
- `docs/hardware/lilygo.md`
- `docs/hardware/lilygo-t-a7670g-r2.md`
- `docs/hardware/lilygo-can-sn65hvd230.md`

## Image migration

- `docs/images/branding` → `docs/assets/images/branding`
- `docs/images/diagrams` → `docs/assets/images/diagrams`
- `docs/images/screenshots` → `docs/assets/images/dashboard`
- obsolete Markdown files inside `docs/images/hardware` removed
- macOS metadata files removed

## Kept intentionally

- `docs/architecture/` for higher-level system views
- `docs/dashboard/` for the end-user dashboard
- `docs/configuration/` pending a later content merge
- `docs/testing/` as historical test notes
- `docs/development/` pending consolidation with `docs/developer/`

These remaining overlaps should be reviewed in DOK-004 rather than deleted blindly.
