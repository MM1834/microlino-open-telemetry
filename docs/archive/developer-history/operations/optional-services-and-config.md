> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# Optional Services and Configuration Management

MOT v1.0.2 adds firmware polish for optional services and configuration handling.

## Stable device name

The firmware now has a stable `deviceName`, used for the MQTT client ID:

```text
mot-<deviceName>
```

If no device name is configured, the firmware uses a stable MAC-based hostname.

## Optional MQTT

MQTT is only active if an MQTT host is configured. If the host field is empty, no connection attempt is made and no MQTT error is printed.

## Optional ABRP

ABRP is only active when both fields are configured:

- ABRP API Key
- ABRP User Token

v1.0.2 sends a minimal stable telemetry set: SoC, speed, estimated range and charging state.

## Configuration Management

The Web UI offers config export/import as JSON and factory reset.

The exported configuration can include secrets such as WiFi and MQTT passwords. Keep backup files private.
