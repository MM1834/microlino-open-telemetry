# Firmware architecture

## Boot sequence

```mermaid
flowchart TD
    Boot[ESP32 boot] --> Config[Load Preferences]
    Config --> Modem[Initialize modem on LilyGO]
    Modem --> GPS[Initialize GPS]
    GPS --> CAN[Initialize TWAI/CAN]
    CAN --> AP[Start local access point]
    AP --> Network[Try configured WiFi, then LTE]
    Network --> MQTT[Initialize MQTT]
    MQTT --> Services[ABRP and optional services]
    Services --> WebUI[Start WebUI and REST API]
    WebUI --> Ready[Runtime loop ready]
```

The exact order differs slightly between platforms, but configuration, communications and local recovery access are initialized before normal telemetry publishing.

## Runtime components

```mermaid
flowchart TB
    subgraph Inputs
      CAN[CAN frames]
      GPS[GPS NMEA]
      Config[Stored configuration]
    end

    subgraph Core
      Decoder[Decoder engine]
      Telemetry[Telemetry state]
      Network[Network manager]
    end

    subgraph Outputs
      MQTT[MQTT]
      API[REST API]
      UI[WebUI]
      ABRP[ABRP]
    end

    CAN --> Decoder --> Telemetry
    GPS --> Telemetry
    Config --> Network
    Config --> MQTT
    Telemetry --> MQTT
    Telemetry --> API
    Telemetry --> UI
    Telemetry --> ABRP
    Network --> MQTT
    Network --> API
```

## Shared telemetry model

Hardware-specific modules should not publish directly from raw input. They first update the shared telemetry model. This keeps MQTT, WebUI and future integrations independent from board-specific CAN or GPS implementations.

Typical groups are:

- `display`
- `charging`
- `location`
- `system`

## Platform separation

Shared definitions live under `firmware/common/`. Board-specific implementations live under:

```text
firmware/esp32-wroom/
firmware/lilygo-t-a7670/
```

This separation allows the decoder and topic model to remain common while network, modem, GPS and WebUI details can differ.
