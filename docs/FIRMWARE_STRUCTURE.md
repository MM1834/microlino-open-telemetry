# Firmware Structure

MOT firmware is split into reusable common modules and board-specific targets.

```text
firmware/
  common/
    app/
    config/
    network/
    mqtt/
    telemetry/
    can/
    decoders/
    api/
    web/
    abrp/
    ota/
    system/

  esp32-wroom/
    platformio.ini
    include/board_config.h
    src/main.cpp

  lilygo-t-a7670/
```

## Principle

Board-specific code defines pins, hardware capabilities and startup wiring.

Common code implements the platform logic:

- telemetry model
- CAN frame dispatching
- decoders
- MQTT publishing
- JSON API
- web configuration
- ABRP upload
- OTA

## Rule

Decoders must not publish MQTT, call ABRP, or render web pages.

Decoders only write to the telemetry model.

Outputs only read from the telemetry model.
