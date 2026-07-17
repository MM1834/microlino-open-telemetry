# GPS-1 — Shared `MotGps` library and ESP32-WROOM hardware test

## Goal

Validate the directly soldered RAK12501/L76K module before changing the
production ESP32-WROOM telemetry firmware.

This sprint also moves the existing LilyGO TinyGPS++ implementation into a
shared library:

```text
firmware/shared-libs/MotGps/
```

## Electrical connection

The GNSS module uses 3.3 V UART logic.

```text
RAK12501 TX -> ESP32 RX
RAK12501 RX <- ESP32 TX
RAK12501 GND -> ESP32 GND
RAK12501 3V3 -> ESP32 3V3
```

Default test pins:

```text
ESP32 RX: GPIO16
ESP32 TX: GPIO17
Baud:     9600
```

RX and TX are named from the receiving device's perspective, so the lines must
be crossed.

Do not connect the module to 5 V.

## Why a test target first?

Direct soldering and undocumented test pads introduce hardware uncertainty.
The isolated test firmware verifies:

- electrical power
- common ground
- UART direction
- UART baud
- NMEA reception
- satellite fix
- GNSS UTC
- antenna operation

Only after this test passes should GPS be added to the production AWS
telemetry target.

## Apply

```bash
unzip ~/Downloads/gps-1-shared-motgps-and-esp32-test.zip
./tools/apply_gps_1_shared_library.py
```

## Build

```bash
cd firmware/esp32-wroom
pio run -e esp32dev-gps-test
```

## Flash and monitor

```bash
pio run -e esp32dev-gps-test -t upload
pio device monitor
```

## Expected stages

### No UART data

```text
"seen":false
Waiting for NMEA data
```

Check power, ground, crossed UART lines and baud.

### NMEA data, no fix

```text
"seen":true
"valid":false
NMEA received, waiting for fix
```

Move the antenna outdoors with a clear view of the sky.

### Valid fix

```text
"seen":true
"valid":true
"satellites":...
"latitude":...
"longitude":...
```

The test firmware also sets ESP32 system UTC from valid GNSS date/time.

## Pin override

When different GPIOs were soldered, edit the three build flags in:

```text
[env:esp32dev-gps-test]
```

Example:

```ini
-D MOT_GPS_RX_PIN=4
-D MOT_GPS_TX_PIN=5
-D MOT_GPS_BAUD=9600
```

## Next sprint

After successful validation:

- integrate `MotGps` into the ESP32 production target
- publish `location/*` through `MotAwsIot`
- expose GPS diagnostics in the WebUI
- use GNSS UTC as an additional time source
- confirm location in DynamoDB, Vehicle API and Dashboard
