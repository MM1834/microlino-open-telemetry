# LG-CAN2-001 — LilyGO Mobile Dual-CAN Pilot

> **Status:** Active — repository implementation complete; hardware acceptance pending
>
> **Started:** 2026-08-21

## Objective

Retain the three available LilyGO T-A7670G modules as bounded mobile pilot
adapters by adding a second passive CAN input through an Adafruit MCP2515 CAN Bus
FeatherWing. Preserve the validated WiFi/LTE AWS IoT path, telemetry contract,
per-device credentials, vehicle isolation, GPS, authenticated local WebUI and
dual-slot local OTA.

This is a sustain-only pilot variant. It does not replace the nanoESP32-C6-N16 as
the strategic firmware and dual-CAN target.

## Scope

- keep CAN1 on the ESP32 TWAI controller and connect it to Standard-CAN;
- add CAN2 through MCP2515 over the unused LilyGO SD SPI pins and connect it to
  Display-CAN, matching the C6 logical mapping;
- keep both channels receive-only with independent decoder profiles;
- adopt the proven 4 MB dual-OTA partition layout used by the C6 compatibility
  target;
- expose CAN2 configuration and bounded diagnostics locally;
- build both LilyGO environments and regress AWS/LTE source contracts.

## Explicit exclusions

- no ABRP-over-LTE work;
- no telemetry topic changes beyond the existing CACHE-001 backfill contract;
- no SD-card support while CAN2 uses the SD SPI pins;
- no production qualification or general LilyGO feature-development restart.

## Hardware contract

### FeatherWing preparation

- `TERM` must be electrically open because both Microlino buses are already
  terminated. The pilot board was prepared with `TERM` cut before wiring.
- `SLNT` must be tied to 3.3 V. This disables the TJA1051/3 transmitter while the
  receiver remains active.
- Firmware independently places the MCP2515 protocol controller in listen-only
  mode. Hardware silent mode and controller listen-only are both required.
- Do not insert an SD card.

### Proposed wiring

| FeatherWing signal | LilyGO GPIO | Purpose |
|---|---:|---|
| SCK | 14 | Existing unused SD SPI clock |
| MOSI | 32 | Safe output; reassigned from the former CAN1 RX |
| MISO | 39 | Safe input-only GPIO; avoids the GPIO2 boot strap |
| CS | 18 | MCP2515 chip select |
| INT | 34 | Receive interrupt/diagnostic input; input-only GPIO |
| RST | 3.3 V | Hold MCP2515 reset inactive in standalone use |
| SLNT | 3.3 V | Permanent physical transmit disable |
| 3V | 3.3 V | FeatherWing logic/charge-pump supply |
| GND | GND | Common logic and CAN reference |
| CANH/CANL | selected second vehicle bus | Passive tap |

GPIO2 was rejected during the first physical boot test: the MCP2515 MISO level
changed the ESP32 boot strap and produced `HSPI_FLASH_BOOT`/download-mode starts.
The final mapping therefore uses input-only GPIO39 for MISO. GPIO2 must remain
unconnected to the FeatherWing.

GPIO15 was rejected during the next physical boot test. With MCP2515 MOSI
attached, the ESP32 entered `HSPI_FLASH_BOOT`; with that wire open it returned to
normal `SPI_FAST_FLASH_BOOT`. The final mapping moves CAN1 RX from GPIO32 to
input-only GPIO36 and uses the released GPIO32 as MCP2515 MOSI. GPIO15 must
remain unconnected to the FeatherWing.

The FeatherWing `RST` pad must be tied to 3.3 V for standalone use. A normal
Feather host holds this active-low line high; when left floating on the LilyGO
wiring, SPI register initialization returned MCP2515 error 1. With `RST=3.3 V`,
the controller entered listen-only mode successfully.

CAN1 uses GPIO36 RX / GPIO13 TX with its existing transceiver and is connected
to Standard-CAN. Its TXD must remain recessive/disconnected according to the REV7
receive-only hardware contract. CAN2/MCP2515 is connected to Display-CAN. The
first boot migrates older LilyGO profile defaults once to CAN1 Standard-CAN V1
and CAN2 Display-CAN; both profiles remain independently configurable afterward.

## Flash and partition boundary

The pre-sprint 2026-08-21 AWS baseline uses 1,307,804 of 1,310,720 application bytes
(99.8%). The existing default table wastes 1,408 KiB on LittleFS and provides
only 1,280 KiB per OTA slot.

The pilot adopts two 1,664 KiB OTA slots, 640 KiB LittleFS and a 64 KiB coredump
partition. The unchanged baseline then occupies 76.8% of one application slot
with 396,132 bytes available before MCP2515 integration.

Changing the partition table requires a controlled USB migration. Back up local
configuration and per-device AWS material first, then write bootloader, partition
table, application and a valid LittleFS image before restoring the unique device
credentials. Do not attempt this migration through the former OTA layout.

## Acceptance gates

- both application slots remain below the 85% pilot gate;
- CAN1 and CAN2 run in software listen-only mode and no application transmit API
  is introduced;
- FeatherWing `SLNT=HIGH` and open termination are physically checked;
- both channels receive simultaneously at 500 kbit/s with independent profiles;
- CAN loops remain responsive during WiFi/LTE transition and AWS TLS reconnect;
- AWS Thing and certificate remain unchanged; the vehicle namespace is migrated
  from shared `pioneer` to isolated `pioneer-lilygo`;
- local WebUI, backup/restore, GPS control and OTA regressions pass;
- the optional cache remains disabled by default, uses trustworthy UTC only and
  uploads only after a fresh live AWS publication;

## Validation status

The `T-A7670X-AWS` build passes with MCP2515 and cache support at 1,332,988 of
1,703,936 application bytes (78.2%) and 56,048 of 327,680 RAM bytes (17.1%). CAN1/CAN2
configuration, backup/restore, diagnostics, bounded receive loops and both
software listen-only modes are implemented. The later pilot decision added the
bounded CACHE-001 SOC/Speed journal because the measured OTA margin remained
adequate; vehicle identity is separated as `pioneer-lilygo`.

The non-AWS `lilygo-t-a7670` build also passes at 1,315,588 application bytes
(77.2%) and 55,600 RAM bytes (17.0%). The focused dual-CAN, listen-only and local
security regression set passes 15 tests; the complete repository suite passes
167 tests and `git diff --check` passes.

Static regression results are recorded with the repository handoff. Bench SPI
discovery and controlled USB partition migration now pass on `mot-lilygo-fe8ce0`.
The device booted with CAN1/TWAI as Standard-CAN and CAN2/MCP2515 as Display-CAN,
both at 500 kbit/s in listen-only mode; retained NVS, the migrated LittleFS AWS
identity, WiFi and local WebUI initialized. WiFi/LTE switching, mobile-network
interruption recovery and cold restart passed on the first road test. The live
Thing metadata, least-privilege IoT policy and confirmed portal-owner assignment
now use `pioneer-lilygo`. The Foundation History and Backfill allowlists were
deployed additively without resource replacement and reached `UPDATE_COMPLETE`.
The final cache-capable firmware and rebuilt LittleFS credentials were flashed
over USB with verified hashes; NVS was preserved. The device booted both CAN
channels and loaded client `mot-lilygo-fe8ce0` with vehicle `pioneer-lilygo`.
Dual-bus vehicle reception, cache outage/replay
and OTA-after-migration evidence remain pending hardware gates.
