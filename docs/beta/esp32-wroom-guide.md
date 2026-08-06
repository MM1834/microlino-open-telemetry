# ESP32-WROOM Beta Guide

> **Status:** Draft from source; user workflow not hardware-validated
>
> **Audience:** ESP32-WROOM beta user and first-line support

## Scope

This guide covers the intended ESP32-WROOM AWS beta device with optional GPS. It
starts after a maintainer has assembled, provisioned and flashed the device.
Beta users must not handle AWS private keys or firmware flashing during initial
setup unless a separate support procedure explicitly authorizes it.

## Before connecting

The handoff package must identify:

- device ID and label;
- assigned vehicle ID/name;
- whether GPS hardware is installed;
- expected firmware version/build variant;
- support contact and recovery boundary;
- whether an OTA password has been set.

Do not include AWS private keys, certificates, WiFi passwords or portal tokens in
the user handoff document.

## Local setup network

With no working WiFi configuration, source starts an open fallback AP named:

```text
MOT-<six-character device suffix>
```

The AP has no password. Configure the device in a private location and disconnect
from it after setup. The local WebUI has no application login.

Connect to the AP and open the gateway address shown by the client/network. The
firmware does not explicitly configure a custom AP address. Do not publish a fixed
address until hardware validation confirms the actual client workflow.

## Wizard behaviour

The local wizard has seven steps:

1. Welcome
2. Detected hardware
3. Network
4. Vehicle and CAN profile
5. Telemetry services
6. Validation
7. Finish

Completing onboarding only stores `onboardingComplete=true` and prevents automatic
wizard launch. It is not proof that WiFi, CAN, AWS or GPS works.

## Required beta configuration

### Vehicle

- Keep the provisioned device name unless support instructs otherwise.
- Keep the assigned `vehicleId`; it controls `mot/<vehicleId>/...` topics.
- Select Display CAN for the currently supported vehicle/bus.
- Do not select Standard-CAN V1 - Pioneer or Standard-CAN V2 expecting data; both
  decoders are intentionally empty pending verified identifiers and scaling.

### Network

Enter the beta user's WiFi/hotspot SSID and password. Saving causes a reboot. The
device waits up to approximately 40 seconds for WiFi and otherwise returns to its
open fallback AP.

When station WiFi connects, firmware requests NTP time and advertises a MAC-derived
`.local` hostname. mDNS availability varies by client network and is not the only
recovery path.

### Services

- AWS IoT is intended to be enabled for the beta build.
- Legacy MQTT should remain disabled unless explicitly used for a diagnostic test.
- ABRP is optional and requires both API key and user token.

The local MQTT test and System Health MQTT result use the legacy MQTT host fields;
they do not prove the AWS IoT X.509 connection. AWS confirmation requires separate
device logs/cloud evidence during validation.

## GPS variants

### Without GPS

GPS is optional. Expected state is not detected/no fix. This must not be treated as
a device failure if the handoff label says “without GPS”.

### With GPS

The receiver is only marked detected after checksum-valid NMEA input. Detection and
position fix are separate. First fix may require appropriate antenna placement and
sky view. Exact timing remains to be hardware-validated.

## Status checks

The local status page exposes:

- firmware version and device ID;
- network mode/IP;
- current Display-CAN telemetry state;
- system-health results;
- optional GPS state;
- links to configuration and local OTA.

CAN “waiting” may mean a sleeping vehicle, wiring/interface problem or no supported
frames. It does not identify the cause by itself.

## Backup

Configuration export contains local secrets, including WiFi/MQTT passwords and
possibly ABRP/OTA credentials. Store it privately. Never attach an unredacted export
to a GitHub issue, email thread or chat.

## Local OTA

OTA routes are available in source regardless of the stored `otaEnabled` field. If
the OTA password is empty, upload is permitted without authentication from the
local network. Beta devices therefore require a non-empty OTA password and private
handling of that password.

Only a binary approved for the exact target and release may be uploaded. The
firmware's page still shows a legacy `esp32dev` example path, so filename/path text
is not proof that the binary is the AWS build.

## Factory reset

Factory reset clears Preferences/NVS configuration and reboots. Source does not
erase LittleFS AWS credentials. A reset therefore does not revoke or remove the
device's cloud identity and must not be used as an ownership-transfer procedure.

## When to contact support

- the expected AP does not appear;
- WiFi repeatedly falls back after verified credentials;
- device ID/vehicle ID differs from the handoff record;
- supported Display-CAN remains waiting with vehicle awake;
- AWS telemetry is absent after WiFi/time recovery;
- OTA fails or firmware version does not change;
- a GPS-equipped unit never reports detected NMEA data.

Use the [safe diagnostic data](safe-diagnostic-data.md) rules before sharing output.
