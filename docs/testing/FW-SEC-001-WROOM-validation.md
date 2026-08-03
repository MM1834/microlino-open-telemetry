# FW-SEC-001 ESP32-WROOM validation

> **Status:** Code/build passed; hardware validation pending
>
> **Revision:** `91045ec`, 2026-08-03

## Automated evidence

- 58 tool tests passed, including seven WROOM local-security checks.
- Nine foundation authorization tests passed.
- Six onboarding-handler tests passed.
- Documentation audit found no structural failure; the existing ignored
  `docs/.DS_Store` metadata warning remains.
- `git diff --check` passed.

## Build evidence

Command:

```bash
cd firmware/esp32-wroom
pio run -e esp32dev-aws
```

Result: success.

| Metric | Result |
|---|---:|
| RAM | 50,864 / 327,680 bytes (15.5%) |
| Application flash | 1,146,065 / 1,310,720 bytes (87.4%) |

No erase, firmware upload, filesystem upload or credential operation was performed.

## Implemented checks

- Valid local password is 12–63 printable ASCII characters.
- Provisioned fallback AP receives the same unique password as WPA2 credential.
- First setup exposes only setup GET/POST; operational handlers fail closed.
- Operational WebUI and APIs use local Basic authentication with user `admin`.
- Mutating handlers require a matching Origin or Referer host.
- OTA requires enabled state, authentication and same-origin upload.
- Configuration HTML does not echo stored WiFi, MQTT, ABRP or admin secrets.
- Blank secret form values preserve the stored values.
- Default configuration export excludes passwords, ABRP key/token and admin secret.
- Configuration import cannot remove the local admin password boundary.

## Pending hardware sequence

1. Identify the exact previously unused WROOM and serial port read-only.
2. Record its MAC/device identity before destructive operations.
3. Obtain explicit approval for that exact erase target and commands.
4. Erase flash and partitions over USB.
5. Flash the exact reviewed AWS firmware and provision its individual AWS files.
6. Join the temporary first-setup AP in a controlled location.
7. Set WiFi plus a unique 12–63 character local admin password and reboot.
8. Confirm the fallback AP is WPA2-protected and unauthenticated routes return
   `401` or fail closed.
9. Confirm authenticated status/config, secret-free export, reset denial and OTA
   disabled state.
10. Enable OTA temporarily and validate an authenticated same-origin update only
    if the release review accepts the 87.4% partition usage.
11. Validate AWS connection and telemetry before portal claim issuance.

The password itself must be transferred outside Git, screenshots and project logs.
