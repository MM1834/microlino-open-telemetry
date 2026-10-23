# MOT Display C6

Supported release-1 local instrument display for the Waveshare
`ESP32-C6-LCD-1.28-B` (SKU 35451). It is intentionally separate from the
vehicle adapter firmware and is installed or updated through its dedicated
controlled WebFlasher targets.

## 1.85-inch touch prototype

The independent PlatformIO environment
`waveshare-esp32-s3-touch-lcd-185c-v2` targets the Waveshare
ESP32-S3-Touch-LCD-1.85C V2 (360 x 360, 16 MB flash, 8 MB PSRAM).
It reuses the versioned MOT BLE contract and combines the normal power view
with speed, indicators, gear, parking brake, light/mode candidates and local
warning states. Prototype PRE7 adds a display-local manual speed warning and
the 1.28-inch display's journey, traction-battery and charging views; it
does not change the adapter, BLE records or canonical telemetry.

- Tap the driving screen or press the side key to open manual speed control.
  Choose `30`, `50`, `60`, `80`, `OFF` or toggle sound muting; every choice
  returns directly to the driving screen. Values follow the selected km/h or
  mph presentation unit.
- Further page changes open MOT status and presentation settings. Tap the
  language, units or source
  row to cycle it; tap the lower return area to return to the driving view.
- Language supports German, English, French and Italian. Metric presentation
  uses km and km/h; UK presentation uses mi and mph. Power and energy remain kW
  and kWh in both systems.
- Presentation settings are stored locally in display NVS. Source `MOT SYNC`
  additionally writes them over the encrypted bonded BLE link to the adapter,
  which persists and republishes the additive `display-config-v1` record. Source
  `LOCAL` ignores adapter changes. This path works without WiFi or AWS and leaves
  a stable adapter-owned seam for later portal synchronization.
- Hold the status screen for three seconds to clear the fixed MOT pairing.
- The target is intentionally separate from the released 1.28-inch C6 build.

The normal page order is driving, speed control, current journey, traction
battery, charging, MOT status and settings. While charging, the display changes
home automatically to the charging page and follows the 1.28-inch priority
order: charging, battery, journey, status, settings, driving and speed control.
Non-home pages return to the state-dependent home after 15 seconds; MOT status
retains its 30-second service window. The power label is rendered from the
wire-format integer tenths rather than LVGL floating-point formatting.

PRE8 gives the charging home a large green power value, a larger SOC value and
a 270-degree SOC progress arc whose active colour moves from red through amber
and yellow to green at 80 percent. MOT status shows both configured WiFi
profiles, then IP and setup AP, followed by a visually separated AWS state.
Presentation settings are read-only on entry; a 1.2-second touch or GPIO0 hold
toggles the red/green locked state before rows can change. Parking brake and
gear are moved inward on the driving page for the circular viewport. PRE9
replaces the temporary text chevrons with larger built-in LVGL left/right arrow
symbols and lowers them slightly toward the 1.28-inch instrument position.
PRE10 splits MOT status into independent WiFi 1, WiFi 2, IP/AP and AWS labels.
The active WiFi profile and a connected AWS state use the existing green
success colour; inactive and disconnected states remain neutral.

PRE11 adds the optional encrypted `charge-summary-v1` characteristic. With a
REV45 adapter, the charging page changes to `LETZTE LADUNG` while idle and shows
the last locally persisted charged energy, SOC start/end and duration. Existing
telemetry records and older 1.28-inch displays remain wire-compatible.

PRE12 adds the optional encrypted `journey-altitude-v1` characteristic from a
REV46 adapter. The 1.85-inch current-journey footer shows accepted live GNSS
altitude and, after two bounded samples, cumulative `+ ascent / - descent`.
Missing or stale altitude is hidden. Values follow metric/UK presentation while
the BLE record remains canonical metres.

PRE13 adds the shared MOT vector image as the final 1.85-inch page after
settings. The matching 1.28-inch `1.2.0-RC7` candidate places it after MOT
status; clockwise and counter-clockwise non-touch navigation include the new
sixth page.

PRE14 and the matching 1.28-inch `1.2.0-RC8` candidate use the complete BLE
re-pair reset: the stored peer address and the local NimBLE bond are both
cleared before scanning resumes. The 1.85-inch status page now keeps the
localized re-pair gesture visible and confirms a successful reset briefly.
The 1.28-inch non-touch status page retains its existing two-stage rotation
gesture, instruction and confirmation screen.

The 1.28-inch `1.2.0-RC9` candidate adds an optional encrypted
`software-versions-v1` record and a dedicated `SOFTWARE-VERSIONEN` page for VCU,
IPC and BDU. Missing values render as `--`; no CAN request is involved. The
target builds at 953,890 bytes flash (72.8% of its application partition) and
21,396 bytes RAM (6.5%), leaving sufficient reserve for the screen.

DualEye `0.1-PRE9` and the 1.85-inch `0.4.1-PRE15` add the same page to their
touch cycles. Their builds use respectively 15.3%/9.4% and 21.5%/31.0% of the
reported flash/RAM budgets, so the page is retained on all three display targets.

## DualEye 1.28-inch touch prototype

The `waveshare-esp32-s3-dualeye-touch-lcd-128` environment targets the
Waveshare ESP32-S3-DualEye-Touch-LCD-1.28. PRE2 reads and acknowledges both
CST816 controllers after their GPIO5/GPIO7 interrupts, so a tap on either eye
advances the right-hand page cycle. The board's GPIO0 BOOT key provides the same
debounced page action and also exits the timed two-image MOT screen.

PRE3 makes that two-image MOT presentation the regular final entry of the
right-hand page cycle instead of exposing it only through `WEITER` on the manual
speed-control page. It remains bounded to 11 seconds and either touch surface or
the GPIO0 key can dismiss it immediately.

PRE4 moves the synchronous BLE connect, secure-connection and GATT discovery
sequence out of the Arduino UI loop into a dedicated FreeRTOS worker. A slow or
failed secure handshake therefore no longer suspends rendering, both touch
controllers or the GPIO0 page key while reconnection continues in the
background.

PRE5 adds the previously missing local re-pairing action. Only on the `MOT
STATUS` page, holding GPIO0 for three seconds clears the fixed adapter assignment
and opens the existing ten-minute pairing window. A short press still advances
the page; releasing after the accepted service hold does not also advance it.

PRE6 replaces repeated CST816 interrupt handling with one bounded touch-contact
state. A short contact changes exactly one page on release, while a continuous
three-second touch on `MOT STATUS` performs the same re-pairing action as the
GPIO0 hold. Holding either touch surface on another page can no longer race
through the complete page cycle.

PRE7 adds the compact `3S TOUCH/TASTE: KOPPELN` service hint directly to the
`MOT STATUS` page so the otherwise hidden re-pairing action remains discoverable.

PRE8 makes that explicit re-pairing action complete: it clears both the fixed MOT
address in display preferences and the ESP32 BLE bond store. Normal reconnects
retain their bond; only the confirmed three-second status action removes keys.
This prevents a stale key from repeatedly producing `secureConnection rc=13`
when the display is deliberately moved from one adapter to another.

After the selected limit plus a canonical tolerance of 2 km/h at 30 or 3 km/h
at higher limits has been exceeded continuously for 1.5 seconds, a ring around
the speed flashes orange and later red. The onboard ES8311/I2S speaker also
emits a short repeated warning beep unless muted. Audio initialization is
fail-soft: an unavailable codec or speaker leaves the visual warning active.

Build and flash:

```sh
pio run -e waveshare-esp32-s3-touch-lcd-185c-v2
pio run -e waveshare-esp32-s3-touch-lcd-185c-v2 -t upload --upload-port /dev/cu.usbmodem...
```

The first inspected V2 sample (MAC `b8:1f:3f:d1:9f:c0`) was backed up before
flashing as `outputs/mot-display-s3-185c/factory-b81f3fd19fc0-20261015.bin`.
Its SHA-256 is
`cb8d122235e0bc64deac8a5faad03120d0d0ac6ff599780fc78e3e1f964440be`.

The BLE display renders live values from the optional N16 display service:

- signed traction, regeneration and charging power;
- segmented power ring;
- state-dependent colour;
- SOC;
- vehicle speed and left/right/hazard indication on the standard driving page;
- estimated distance to a target SOC;
- explicit driving/charging state.

The rendering boundary accepts a `MotDisplaySample`; the BLE client remains
decoupled from the N16 CAN decoder through the shared versioned 16-byte packet.
It discovers the service automatically, establishes a bonded BLE Secure
Connections link, subscribes at up to 5 Hz, validates the protocol version and
CRC-16, rejects stale data after 2.5 seconds and resumes scanning after a
disconnect. Until a compatible adapter is reachable, the display shows an
explicit connection state rather than synthetic vehicle values.
Frames are composed in one 240 × 240 RGB565 canvas and flushed as a complete
image; the panel is never cleared visibly between drawing operations.

Protocol v1 carries canonical signed power (consumption positive,
regeneration/charging negative), state flags, SOC, distance to target SOC,
source age and a monotonically increasing sequence. The initial live stream uses
the Display-CAN range estimate to 0%; other target-SOC projections remain later
display work.

The display presentation deliberately differs from the canonical wire sign only
for charging: charger power is shown as a positive magnitude, while driving
regeneration retains its negative sign and traction consumption remains positive.
This is a rendering rule only; it does not change protocol v1 or adapter data.

## Boot-selected standalone Display-CAN

Display 1.2.0 adds an optional local, receive-only Display-CAN source without
removing or changing the existing BLE protocol. `GPIO2` is sampled once with an
internal pull-up during boot. A CAN harness that pulls it to GND selects local
TWAI for the complete runtime; otherwise the display starts the existing BLE
client. The firmware never switches or combines sources after boot.

Standalone TWAI receives at 500 kbit/s on `GPIO1`. The driver reserves `GPIO3`
as its unused TX signal, but that pin must not be wired to the transceiver. The
transceiver TXD is instead held recessive at 3.3 V through 10 kΩ, establishing a
hardware receive-only boundary. Wiring and the non-isolated/isolated CAN choice
are specified in
[Standalone Display-CAN wiring](../../docs/hardware/mot-display-standalone-can.md).

`GPIO12` and `GPIO13` remain reserved for the board's native USB D-/D+ signals.
They must not be used by the standalone harness or TWAI configuration.

The standalone main page uses the shared Display-CAN decoder for SOC, speed,
range, local power compatibility value, indicators/hazards, parking brake and
accepted warnings. Trip and odometer are decoded from their DBC bit fields.
N/D/R, position/low-beam and Sport/Eco are rendered as validation candidates;
they remain local and must not be promoted to cloud telemetry before controlled
vehicle confirmation. Adapter-only battery and network values remain explicitly
unavailable in standalone mode. Because the local `0x603` power value did not
produce useful charging measurements in the physical pilot, the standalone
charging page deliberately shows only SOC and a green charging bar. It does not
show or integrate local power. This does not alter the BLE adapter path or its
full numeric charging screen.

A second adapter-status packet is sent at 1 Hz and contains only bounded WiFi
profile names, configured/active/online state, RSSI, local IP and MOT Cloud/AWS
connection state plus whether the local setup AP is active. It never carries WiFi passwords, certificates or other
secrets. Deliberately rotating the enclosure left or right selects a page; normal
pages return to the driving or charging home page after 15 seconds. The status
page remains available for 30 seconds so its service gesture cannot race that
automatic return. Pairing reset is a two-stage service gesture available only
on the adapter-status page: hold either direction for three seconds, return the
display to neutral, then hold the opposite direction for three seconds within
the ten-second confirmation window. Only then is the fixed adapter
assignment cleared and a new two-minute pairing window opened. A two-second
confirmation remains visible while BLE already searches and reconnects in the
background. Detection samples the onboard QMI8658 accelerometer over I2C
(`SDA=18`, `SCL=8`, address `0x6B`) without using its shared interrupt pins. The
relative tilt is calibrated at startup, filtered, checked against plausible
gravity and jerk, and requires a return to neutral before another gesture. This
avoids exposing re-pairing to ordinary page changes or vehicle vibration.
BLE discovery runs asynchronously; its repeated three-second search window must
never suspend IMU sampling, page rendering or gesture confirmation.

Release 1 adds an optional 12-byte driving-status characteristic without changing
the established telemetry, status or extended records. It carries fresh
Display-CAN speed and the `0x603 data[2]` direction-indicator and hazard bits. The main driving page
always reserves the prominent speed position (`-- km/h` before valid data) and
retains power below it. The DBC mapping interprets `0x02` as right, `0x04` as
left and `0x08` as hazards in `data[2]`; hazards activate both arrows.
Physical left/right/hazard acceptance passed on 2026-10-13. This complete
emergency-instrument view is the default driving page on the 1.28-inch display.
On future larger touch displays it is intended as an explicit fallback mode,
not the normal operating page; normal mode should omit speed and indicators.

REV43/Display 1.2.0-RC4 retain the established 14-byte driving-status layout
introduced with the local warning fields. Seven reserved flag bits carry gear,
the two raw light bits, the two raw Eco/Sport bits and one shared validity marker,
so older adapters remain fail-closed and no record-length migration is required.
Physical testing maps the first raw state to the vehicle switch label `LICHT 0`.
The second state remains active for both higher switch positions and therefore
cannot distinguish them; the display labels it `LICHT 2` only as raw evidence.
Fernlicht does not affect either currently decoded bit. RC4 consequently shows
`LICHT 0`, `LICHT 2` or `LICHT 0+2` without claiming position, low-beam or
high-beam semantics. These candidate values remain local to the encrypted
display link.

On a display without an existing assignment, startup opens a two-minute pairing
window. The first adapter that completes encrypted bonding and presents a valid
capability record with telemetry-v1, status-v1 and bonded-encryption support is
stored in NVS. Later scans accept only that BLE address. This prevents the
display from silently following another nearby MOT after initial setup. Because
the boards have no pairing input/output, BLE uses encrypted `Just Works`
bonding without MITM authentication; first pairing should therefore be done in
physical proximity to the intended adapter.

The status page advertises the initial three-second turn. A short tilt followed
by return to neutral remains normal page navigation. The destructive step is
shown only after a continuously held three-second tilt and still requires the
opposite confirmed hold.

Arduino_GFX 1.6.7 is pinned because 1.6.4, still named in early vendor material,
does not compile with the Arduino-ESP32 3.3.6+ SPI API used by MOT's current
toolchain.

PRE2 keeps normal power-state labels at text size 2 but renders long BLE
connection messages at size 1 so they remain inside the round viewport. Its
WebFlash factory package (not an ordinary PlatformIO upload) also places the
MOT Display layout-v1 marker at `0x8C00`; a later application-only WebFlash path
must verify that marker and the bound partition-table hash before writing at
`0x10000`.

Release 1 uses that guarded application-only path. Package it with:

```sh
python3 tools/package_display_webflash_update.py \
  --version MOT-DISPLAY-1.2.0-RC6 \
  --output-dir /tmp/mot-display-1.2.0-rc6-update
```

The resulting target is `waveshare-esp32-c6-lcd-128-b-update`. It preserves the
PRE2 marker, partition table, settings and BLE bond; WebFlash fails closed unless
the live marker and partition table match the release contract.

The accepted release includes the formerly pre-release additive BLE record for
V2 SOH, pack
voltage/current, cell delta, journey distance/duration/net consumption/regen
peak and charged energy. Individual cell voltages are omitted. Charge target SOC
and remaining time show `--` until reliable adapter sources are available.

The page order follows vehicle state. Driving cycles through power/SOC, current
journey, traction battery, charging and adapter status; charging cycles through
charging, traction battery, current journey, adapter status and power/SOC. A
non-home page returns to the state-appropriate home after 15 seconds. Pairing
reset is accepted only while adapter status is still visible; the former
cross-page authorization latch is removed.

PRE4 also hardens raw knock detection against delayed false events. Samples
following a loop stall reset only the acceleration baseline, and an accepted
knock must settle below the quiet threshold within 80 ms. The gesture decision
window is 420 ms. BLE reconnect scanning can still pause the main loop while
disconnected, but stale motion across that pause cannot become a knock. Mounted
road testing still rejected the gesture input: vehicle vibration caused false
page changes and could eventually reach the status page and trigger pairing
reset. Gesture navigation is therefore not release-ready even with status-only
pairing authorization.

## Build and upload

```sh
pio run -d firmware/mot-display-c6
pio run -d firmware/mot-display-c6 -t upload --upload-port /dev/cu.usbmodem1201
pio device monitor --port /dev/cu.usbmodem1201 --baud 115200
```

The matching adapter test image is built separately so normal C6 releases do not
enable BLE implicitly:

```sh
pio run -d firmware/esp32-c6 -e nanoesp32c6-n16-display-ble
```

## Portal WebFlash package

The active release uses PlatformIO's merged `firmware.factory.bin` for the
first installation on an untouched supported display. Package it only through
the contract checker; do not substitute an application-only image:

```sh
python3 tools/package_display_webflash_release.py \
  --version MOT-DISPLAY-1.2.0-RC6 \
  --output-dir build/webflash-display-1.2.0-rc6-factory
```

The packager verifies that the merged image contains the exact separately built
bootloader at `0x0`, partition table at `0x8000`, display application at
`0x10000` and expected embedded display marker. It emits an immutable binary and
manifest for the private, grant-controlled portal catalog. Packaging does not
upload, grant or flash anything.

## Board mapping

| Function | GPIO |
|---|---:|
| LCD DC | 19 |
| LCD CS | 10 |
| LCD SCK | 11 |
| LCD MOSI | 21 |
| LCD reset | 20 |
| LCD backlight | 7 |

## Factory restore

The locally captured complete flash backup is deliberately ignored under
`outputs/mot-display-c6/`. The 16,777,216-byte image captured on 2026-10-10 has
SHA-256 `8cfc3088a8691a5a0a33312f2ede85e5bc430e1a190e2d6f140f29264ceee234`.
Restore it only to the same physical board:

```sh
esptool --chip esp32c6 --port /dev/cu.usbmodem1201 \
  write-flash 0x0 \
  outputs/mot-display-c6/waveshare-esp32-c6-lcd-1.28-b-factory-2026-10-10.bin
```
