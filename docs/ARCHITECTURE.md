# Architecture

Microlino Open Telemetry (MOT) is designed as a modular telemetry platform.

The central idea is simple:

> Inputs decode raw data into one telemetry model. Outputs consume that telemetry model.

## High-Level Architecture

```text
                 Microlino
                     │
        ┌────────────┴────────────┐
        │                         │
   Display CAN               BMS CAN
        │                         │
        └────────────┬────────────┘
                     │
              Decoder Engine
                     │
                     ▼
              Telemetry Model
                     │
      ┌──────────────┼──────────────┐
      │              │              │
    MQTT         JSON API        ABRP
      │              │
 Dashboard      Third-party Apps
```

## Major Components

### Inputs

Inputs receive raw data from hardware or external sources.

Current input:

- ESP32 TWAI CAN input for Microlino Display CAN

Planned inputs:

- second CAN input for BMS CAN
- LilyGO LTE modem status
- GPS receiver
- future cloud or diagnostic sources

### Decoder Engine

The decoder engine dispatches raw input frames to the correct decoder.

Examples:

```text
CAN ID 0x602 → Microlino Display CAN decoder
CAN ID 0x603 → Microlino Display CAN decoder
CAN ID 0x604 → Microlino Display CAN decoder
```

Future BMS decoders will use the same mechanism.

### Telemetry Model

The telemetry model is the central data structure of MOT.

It contains normalized values such as:

- SOC
- speed
- odometer
- range estimate
- charging state
- power
- BMS values
- GPS values
- system values

All outputs read from this model.

### Outputs

Outputs publish or display telemetry data.

Current outputs:

- MQTT
- embedded JSON API
- embedded web configuration/status page

Planned outputs:

- ABRP upload
- external dashboard
- Home Assistant discovery
- OTA status
- cloud bridge

## Firmware Layout

Planned firmware layout:

```text
firmware/
  common/
    app/
    config/
    telemetry/
    can/
    decoders/
    mqtt/
    network/
    web/
    api/
    abrp/
    ota/
    system/

  esp32-wroom/
  lilygo-t-a7670/
```

`common/` contains hardware-independent logic.

Hardware-specific firmware projects include the common code and provide board-specific configuration.

## Display CAN as Baseline

Microlino Display CAN is used as the common baseline because it is available on Pioneer and newer models.

It currently provides confirmed or useful values such as:

- SOC
- odometer
- speed
- charging candidate values

Additional BMS CAN decoders may later enrich the same telemetry model with model-specific values.

## Dual-CAN Strategy

Future versions may use two CAN inputs:

```text
CAN 1 → Microlino Display CAN
CAN 2 → Microlino BMS CAN profile
```

This allows MOT to keep the universal Display CAN baseline while adding richer model-specific BMS data.

## Network Strategy

ESP32-WROOM version:

- WiFi STA mode if configured
- fallback AP if WiFi is unavailable
- local web configuration
- MQTT over WiFi

LilyGO version planned:

- WiFi where available
- LTE fallback or LTE-only mode
- GPS
- cloud MQTT
- ABRP over LTE

## Design Rule

No output module may decode CAN data directly.

If a value is needed by MQTT, Web UI, JSON API or ABRP, it must exist in the telemetry model first.
