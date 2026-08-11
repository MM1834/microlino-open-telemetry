# Firmware Overview

> **Status:** Source-confirmed with bounded WROOM/LilyGO hardware validation
>
> **Audience:** Developer, maintainer and beta-support author
>
> **Evidence date:** 2026-08-02

## Firmware targets

MOT contains three board-specific applications sharing common telemetry, decoder,
configuration, AWS IoT and GPS components:

- `firmware/esp32-wroom/`
- `firmware/lilygo-t-a7670/`
- `firmware/esp32-c6/`

The WeAct ESP32 CAN485 is a hardware/transceiver option in the ESP32 family, not a
third application target in the repository.

## Declared PlatformIO environments

| Environment | Source meaning | DOC-001 status |
|---|---|---|
| `esp32dev` | ESP32-WROOM with legacy plain MQTT path | Legacy build structure |
| `esp32dev-aws` | ESP32-WROOM with LittleFS and `MOT_AWS_IOT=1` | Intended maintained AWS variant |
| `lilygo-t-a7670` | LilyGO with legacy WiFi/LTE MQTT path | Legacy build structure |
| `T-A7670X-AWS` | LilyGO with LittleFS and `MOT_AWS_IOT=1` | Intended maintained AWS variant; WiFi-preferred AWS with LTE/TLS fallback |
| `nanoesp32c6-n16` | Primary 16 MB dual-CAN C6 line with runtime-optional AWS | Maintained pilot/feature target |
| `xiao-esp32c6` | 4 MB unified C6 line with runtime-optional AWS | Compatibility target with strict app-slot size gate |

The standalone ESP32 GPS test environment was removed after GPS became a normal
runtime capability of the WROOM firmware. C6 now has exactly one environment per
board; both include AWS IoT and LittleFS, and remain local-only when no credentials
are provisioned. WROOM and LilyGO retain their legacy pre-AWS environments pending
a separate compatibility decision.

## Shared components

| Path | Responsibility |
|---|---|
| `firmware/common/telemetry` | Normalized display, charging, location and system state |
| `firmware/common/decoders` | Decoder registry, Display-CAN decoder and empty Standard-CAN template |
| `firmware/common/config` | Configuration contract, keys and readiness model |
| `firmware/common/api` | Shared telemetry JSON |
| `firmware/common/abrp` | Optional asynchronous WiFi ABRP client shared by WROOM and C6 |
| `firmware/common/onboarding` | Shared local onboarding steps and navigation |
| `firmware/common/system` | Version and stable device identity helpers |
| `firmware/shared-libs/MotAwsIot` | X.509 MQTT/TLS, presence, heartbeat and telemetry publishing |
| `firmware/shared-libs/MotGps` | NMEA parsing and detected/fix state |

## Runtime assembly

All targets initialize configuration, networking, CAN/GPS, MQTT/AWS, local WebUI
and optional ABRP, then update the shared telemetry state in their main loop.
Board-specific modules still own their transport and WebUI implementations.

```mermaid
flowchart LR
    CAN["TWAI CAN input"] --> Decoder["Selected decoder profile"] --> Telemetry["Shared telemetry"]
    GPS["Optional NMEA GPS"] --> Telemetry
    Config["Preferences / JSON"] --> Services["Network, MQTT/AWS, ABRP"]
    Telemetry --> Services
    Telemetry --> Local["Local WebUI / JSON APIs"]
    Services --> Cloud["Legacy broker or AWS IoT"]
```

## Source-confirmed capability matrix

“Present” means code/configuration exists; it does not mean the current commit has
been built or tested on hardware.

| Capability | ESP32-C6 | ESP32-WROOM | LilyGO T-A7670G |
|---|---|---|---|
| TWAI receive at 500 kbit/s | Two controllers | One controller | One controller |
| Decoder profiles | Shared | Shared | Shared |
| Protected AP and authenticated WebUI | Present | Present | Present |
| Local browser OTA | Present; opt-in | Present; opt-in | Present; opt-in |
| Optional GPS | Board-profile UART | UART RX 16/TX 17 | L76K UART RX 22/TX 21 |
| AWS IoT X.509 | Canonical profiles, runtime-optional, WiFi | `esp32dev-aws`, WiFi | `T-A7670X-AWS`, WiFi/LTE |
| ABRP | Asynchronous WiFi, AWS-compatible | Shared client over WiFi | WiFi only |
| Local onboarding wizard | Present | Present | Present |

## Security boundary

The firmware local WebUI and APIs require the configured local administrator
password in operational state. Both board families use that password for the
operational setup AP and local OTA is disabled by default. A device without a valid
administrator password exposes a bounded open first-setup AP so the provisioner can
establish the boundary. The local interface must not be exposed to the public
Internet.

Portal accounts and device ownership do not belong in this local WebUI.

## Validation boundary

Current WROOM and LilyGO AWS builds, local-security behaviour and bounded physical
paths have validation records under `docs/testing/`. Exact release artifacts,
hashes and extended LilyGO qualification remain separate release gates.

## Related documents

- [Firmware architecture](architecture.md)
- [Local device API](../api/local-device-api.md)
- [MQTT topics](../api/mqtt-topics.md)
- [Hardware comparison](../hardware/comparison.md)
- [Current status](../governance/CURRENT_STATUS.md)
