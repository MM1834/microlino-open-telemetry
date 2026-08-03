# Safe Diagnostic Data for Beta Support

> **Status:** Current handling baseline; technical collection remains draft
>
> **Audience:** Beta user, support and maintainer

## Principle

Share the minimum evidence required for one support case. Treat device identity,
vehicle data, network data and location as potentially identifying even when they
are not authentication secrets.

## Normally safe after review

- firmware version and board/variant name;
- GPS hardware present/absent and detected/fix booleans, without coordinates;
- WiFi connected/disconnected and coarse signal quality, without credentials;
- CAN initialized/valid/waiting and selected profile;
- uptime, reset reason, sanitized error code and timestamp;
- a shortened device suffix sufficient to match the support inventory;
- steps to reproduce and expected versus actual behaviour.

## Redact or omit

| Data | Handling |
|---|---|
| WiFi SSID/password | Omit both from public or routine tickets |
| MQTT host/user/password | Omit credentials; disclose host only privately when required |
| AWS private key and certificate files | Never transmit through ordinary support channels |
| Thing name, certificate ID, endpoint and full MAC/device ID | Keep in controlled maintainer records; redact from public material |
| ABRP API key/user token | Never include |
| OTA password | Never include |
| Portal JWTs, session data, email/account IDs | Never include tokens; minimize personal identifiers |
| Exact GPS coordinates/routes | Omit unless essential and explicitly approved |
| Vehicle ID/name/VIN/registration | Replace with a case alias where possible |
| Public IP and private network details | Redact unless essential to a controlled network case |

## High-risk sources

Configuration export includes secret fields and must be treated as a credential
bundle. Do not attach it unredacted.

System Health output can contain network host/IP information and GPS coordinates.
Its MQTT diagnostic concerns legacy MQTT, not AWS IoT X.509. Review and redact each
field before sharing.

Screenshots can expose browser URLs, WiFi names, vehicle names, coordinates,
password-manager overlays, account email and background notifications. Crop and
redact the source image; do not rely only on a document viewer overlay.

Logs may contain endpoint, client/Thing name, topics, tokens or telemetry. Search
for secrets and identifiers before transfer. When in doubt, send a short manually
prepared excerpt rather than a complete log.

## Support-data template

```text
Case: <support case ID>
Time/timezone: <ISO timestamp and zone>
Device: MOT-<short suffix>
Variant: ESP32-WROOM / GPS yes|no
Firmware: <reported version>
Network: connected|fallback AP|offline
Vehicle state: awake|charging|sleeping|unknown
CAN: initialized <yes|no>, data valid <yes|no>
GPS: hardware <yes|no>, detected <yes|no>, fix <yes|no>
Symptom: <short description>
Steps already tried: <non-secret summary>
Sensitive fields removed: <yes; reviewer initials>
```

Use a case-scoped, access-controlled channel for any exceptional diagnostic data.
Delete temporary copies according to the project's future retention policy; that
policy is not yet defined and remains an onboarding/support prerequisite.
