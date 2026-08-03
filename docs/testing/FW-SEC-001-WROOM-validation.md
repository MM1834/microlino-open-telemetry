# FW-SEC-001 ESP32-WROOM validation

> **Status:** Code/build, AWS hardware provisioning and authenticated status
> validation passed; remaining destructive/admin checks pending
>
> **Revision:** `91045ec`, 2026-08-03

## Automated evidence

- 62 tool tests passed, including nine WROOM local-security checks.
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
| Application flash | 1,147,185 / 1,310,720 bytes (87.5%) |

The initial build evidence was collected without a hardware write. The controlled
hardware sequence below was subsequently performed after explicit approval.

## Hardware evidence: beta-02

Date: 2026-08-03

| Check | Result |
|---|---|
| Exact serial target | `/dev/cu.usbserial-0001` |
| Chip | ESP32-D0WDQ6-V3 revision 3.0 |
| MAC reported by esptool | `78:21:84:9b:19:98` |
| Firmware device ID | `MOT-842178` |
| Vehicle ID | `beta-02` |
| AWS Thing / MQTT client | `mot-esp32-842178-beta02` |
| Full flash erase | Passed |
| `esp32dev-aws` firmware upload and hash verification | Passed |
| Device-specific LittleFS upload and hash verification | Passed |
| First setup, WiFi join and local admin password creation | Passed |
| AWS online state and fresh heartbeat after credential correction | Passed |
| Telemetry without CAN | Plausible neutral values received |
| Authenticated local status reports AWS IoT endpoint on port 8883 | Passed |
| Local status reports AWS IoT connected instead of disabled legacy MQTT failure | Passed |
| No-GPS state distinguishes UART noise from valid NMEA | Passed |

The first provisional Thing name used the last three bytes shown in esptool's
formatted MAC (`9b:19:98`). Firmware `motDeviceShortId()` instead uses the low
24 bits returned by `ESP.getEfuseMac()`, which produce `842178` on this device.
Before portal claim issuance, a corrected Thing and certificate were provisioned,
the device reconnected with the corrected MQTT client ID, and the provisional
Thing, policy, certificate and local credential copies were removed.

The WROOM has no CAN connection during this validation. Therefore `false` charging,
zero power and unknown range are expected; changing vehicle telemetry remains to be
tested on the Microlino. This unit has no GPS receiver. The firmware treats GPS as
optional and reports it as not detected until checksum-valid NMEA data is received.

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

## Hardware sequence and remaining checks

1. [x] Identify the exact previously unused WROOM and serial port read-only.
2. [x] Record its MAC/device identity before destructive operations.
3. [x] Obtain explicit approval for that exact erase target and commands.
4. [x] Erase flash and partitions over USB.
5. [x] Flash the exact reviewed AWS firmware and provision its individual AWS files.
6. [x] Join the temporary first-setup AP in a controlled location.
7. [x] Set WiFi plus a unique 12–63 character local admin password and reboot.
8. [x] Confirm the fallback AP is WPA2-protected and unauthenticated routes return
   `401` or fail closed.
9. [ ] Authenticated status, secret-free configuration display and OTA-disabled
   state passed. Secret-free export and reset denial remain pending.
10. [ ] Enable OTA temporarily and validate an authenticated same-origin update only
    if the release review accepts the 87.4% partition usage.
11. [x] Validate AWS connection and telemetry before portal claim issuance.

The password itself must be transferred outside Git, screenshots and project logs.
