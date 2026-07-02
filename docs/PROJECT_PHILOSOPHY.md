# Project Philosophy

> One telemetry model. Multiple vehicles. Open platform.

Microlino Open Telemetry (MOT) is built around a single architectural principle: all vehicle data is decoded into one shared telemetry model.

This telemetry model is the single source of truth for the whole project.

## Core Principles

## One Telemetry Model

All decoded values are stored in one common telemetry structure.

Examples:

- state of charge
- speed
- odometer
- charging state
- power
- battery values
- GPS position
- system status

Outputs never maintain their own separate interpretation of vehicle data.

## Multiple Inputs

Vehicle data may come from different sources:

- Microlino Display CAN
- Microlino CAN / BMS 1
- Microlino CAN / BMS 2
- GPS
- LTE modem
- future external sensors

Each input can provide or improve parts of the telemetry model.

## Multiple Outputs

The same telemetry model is consumed by different outputs:

- MQTT
- JSON API
- embedded web UI
- external dashboard
- ABRP
- Home Assistant
- Node-RED
- future apps

Outputs do not decode CAN frames.

## Decoder Isolation

Vehicle-specific logic belongs only inside decoders.

A decoder receives raw data and writes normalized values to the telemetry model.

For example:

```text
CAN frame 0x602
    ↓
Microlino Display CAN decoder
    ↓
telemetry.display.soc
```

MQTT does not need to know whether SOC came from Display CAN, BMS CAN or a future data source.

## Confirmed and Experimental Values

MOT should clearly distinguish between confirmed and experimental values.

Confirmed values are suitable for normal users and integrations.

Experimental values are useful for reverse engineering but should be marked as such.

## Open by Design

MOT is not just firmware. It is a platform consisting of:

- firmware
- dashboard
- documentation
- MQTT specification
- JSON API
- hardware guides
- CAN decoder documentation
- future integrations

The goal is to make Microlino telemetry understandable, reproducible and extensible.
