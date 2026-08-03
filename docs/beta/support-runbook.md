# ESP32-WROOM Beta Support Runbook

> **Status:** Draft from source; procedures requiring a device remain unvalidated
>
> **Audience:** First-line support and maintainer

## Support boundary

This runbook covers the ESP32-WROOM AWS beta with optional GPS. Begin with local,
non-destructive observation. Configuration changes, restarts, OTA, factory reset,
credential replacement and AWS changes require explicit authorization from the
device owner or responsible maintainer.

Never request a private key, an unredacted configuration export, WiFi/MQTT/OTA
passwords, ABRP tokens or portal tokens in a support ticket.

## Initial record

Record before troubleshooting:

- support case ID and time/timezone;
- physical device label and only the minimum device-ID suffix needed to identify it;
- GPS or no-GPS hardware variant;
- reported firmware version and board value;
- network mode: station WiFi, fallback AP or unknown;
- vehicle state: awake, charging, sleeping or unknown;
- symptom, first occurrence and last known working state;
- whether any configuration, firmware, harness or cloud assignment changed.

Follow [safe diagnostic-data rules](safe-diagnostic-data.md) for every attachment.

## Triage levels

### Level 0 — Observation

- Check LEDs/display and whether the expected `MOT-<suffix>` AP appears.
- Check whether the vehicle is awake before interpreting missing CAN values.
- For GPS hardware, move to a suitable reception location before interpreting no
  fix; detection and fix are different states.
- Compare device label, hardware variant and expected version with the handoff
  record.

### Level 1 — Local read-only inspection

If local access is safe, record only redacted output from status endpoints/pages:

- `/status` or `/api/status` for version, board and runtime state;
- `/api/readiness` for wizard/configuration flags;
- `/api/gps` for detected/fix state on GPS-equipped devices.

These values are indicators, not end-to-end proof. In particular, the AWS build
flag can make readiness report AWS as configured without proving that credentials
loaded or an X.509 connection succeeded.

`/api/system-health` is not a purely passive AWS check. Its network/MQTT diagnostics
use legacy MQTT configuration and can disclose host, IP and location data. Use it
only when needed, redact it, and do not interpret its MQTT result as AWS evidence.

### Level 2 — Authorized local actions

Obtain confirmation before any of these actions:

- save/import configuration or restart the wizard;
- run MQTT or ABRP connection tests;
- reboot the device;
- upload firmware through local OTA;
- perform factory reset.

Use only a release-approved binary for OTA. Factory reset clears Preferences/NVS
but does not remove LittleFS AWS identity files or revoke their certificate.

### Level 3 — Maintainer/cloud escalation

Escalate when the issue may involve:

- mismatched Thing, certificate, client ID, vehicle ID or topic namespace;
- certificate expiry/revocation, policy or ingestion failure;
- repeated crashes, boot loops or failed OTA;
- wiring, transceiver, power or CAN-bus integrity;
- lost/stolen/returned hardware or ownership change;
- possible credential or personal-data exposure.

Cloud inspection or mutation is outside this runbook and requires separate approval.

## Symptom guide

| Symptom | Safe first checks | Do not conclude yet |
|---|---|---|
| Expected AP absent | Device label, power, nearby SSIDs, prior WiFi availability | That firmware is dead |
| WiFi repeatedly falls back | SSID spelling, signal, client isolation, time of failure | That AWS is the cause |
| CAN remains waiting | Vehicle awake, intended profile, physical connector/harness review | That decoder or ECU failed |
| No GPS shown | Confirm GPS variant; then detected versus fix state | That no-GPS units are faulty |
| WiFi works but AWS data absent | Version, time sync, assignment record; escalate for device/cloud evidence | That legacy MQTT test proves AWS |
| OTA page is open | Stop if OTA password was not set; isolate device and escalate | That `otaEnabled=false` protects it |
| Factory reset did not de-register device | Expected from current source; escalate for identity/revocation workflow | That reset erased certificates |

## Escalation package

Provide:

- concise symptom and reproduction sequence;
- redacted device/version/variant and timestamps;
- relevant status flags or a redacted screenshot;
- exact actions already performed and their results;
- whether the vehicle/device is still available for controlled testing.

Do not attach full configuration backups, certificate files or raw logs before
reviewing them under the safe-data checklist.
