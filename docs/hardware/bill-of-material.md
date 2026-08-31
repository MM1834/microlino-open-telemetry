# Bill of materials

## Common components

| Component | Required | Notes |
|---|---:|---|
| ESP32-based board | Yes | ESP32-WROOM, WeAct CAN485 or LilyGO |
| CAN transceiver | Yes | SN65HVD230 or integrated equivalent |
| OBD/CAN cable | Yes | Vehicle CAN access |
| USB cable | Yes | Flashing, logs and bench power |
| Enclosure | Recommended | Required for end-user-ready installation |

## ESP32-C6-specific components

| Component | Required | Notes |
|---|---:|---|
| DA37+DA10 GNSS module | GPS only | Preferred C6 pilot receiver; 3.3 V UART/NMEA at 9600 baud |
| Separate GNSS antenna | GPS only | Can be positioned independently while the receiver remains inside the enclosure |
| GPS support capacitors | Recommended | Fit the capacitors specified by the C6 Gen.2 assembly guide for final pilot hardware |

## LilyGO-specific components

| Component | Required | Notes |
|---|---:|---|
| SIM card | LTE only | Provider APN required |
| LTE antenna | LTE only | Required for reliable registration |
| L76K GPS module | GPS | Current GPS module |
| GPS antenna | GPS | Needs sky visibility |
