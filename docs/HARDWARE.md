# Hardware

This document describes the currently supported and planned hardware for Microlino Open Telemetry (MOT).

## Current Reference Hardware

The current reference prototype uses:

- ESP32-WROOM DevKit
- SN65HVD230 / VP230 CAN transceiver module
- Microlino Display CAN
- optional 12 V to 5 V step-down converter

## ESP32-WROOM Reference Wiring

### ESP32 to CAN Transceiver

| ESP32-WROOM | SN65HVD230 / VP230 module |
|---|---|
| 3V3 | 3V3 |
| GND | GND |
| GPIO26 | CTX / TXD |
| GPIO27 | CRX / RXD |

### CAN Bus

| CAN Transceiver | Vehicle |
|---|---|
| CANH | Display CAN-H |
| CANL | Display CAN-L |

The current firmware uses ESP32 TWAI in listen-only mode.

## CAN Transceiver Notes

Some SN65HVD230 modules expose an `RS` pin.

For normal high-speed CAN operation:

```text
RS → GND
```

Some compact modules do not expose `RS`. These usually have the pin already configured on the module.

## Termination

Vehicle CAN buses are normally already terminated at both ends.

For a passive listener, the additional CAN module should usually not add another 120 ohm termination resistor.

During initial testing, a module with termination may still appear to work, but the long-term goal should be to avoid unnecessary bus loading.

## OBD-II Power

For vehicle installation, the ESP32 should be powered from OBD-II 12 V using a step-down converter.

Recommended power path:

```text
OBD-II Pin 16  +12 V  → Step-down IN+
OBD-II Pin 4/5 GND   → Step-down IN-

Step-down OUT+ 5 V   → ESP32 5V / VIN
Step-down OUT- GND   → ESP32 GND
```

Do not power the ESP32 DevKit through the 3V3 pin unless the power supply is known to be stable and suitable.

## Recommended Step-Down Converter

Use a 12 V to 5 V buck converter with sufficient margin.

Recommended:

- input range suitable for automotive 12 V systems
- output 5 V
- at least 1 A for ESP32-WROOM
- higher current for LilyGO LTE boards
- fuse on the 12 V input side

## LilyGO Hardware - Planned

Planned target:

- LilyGO T-A7670G or similar ESP32 LTE/GPS board
- SN65HVD230 / VP230 CAN transceiver
- LTE connectivity
- GPS telemetry
- optional external antennas

The LilyGO version should reuse the same common firmware modules.

## Dual-CAN - Planned

Future dual-CAN support may use:

- ESP32 internal TWAI plus external CAN controller
- dual-CAN board
- two independent CAN inputs

Planned strategy:

```text
CAN 1 → Microlino Display CAN
CAN 2 → Microlino BMS CAN profile
```

Display CAN remains the common baseline for all supported vehicles.
