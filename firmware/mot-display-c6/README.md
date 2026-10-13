# MOT Display C6

Supported release-1 local instrument display for the Waveshare
`ESP32-C6-LCD-1.28-B` (SKU 35451). It is intentionally separate from the
vehicle adapter firmware and is installed or updated through its dedicated
controlled WebFlasher targets.

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

On a display without an existing assignment, startup opens a two-minute pairing
window. The first adapter that completes encrypted bonding and presents a valid
capability record with telemetry-v1, status-v1 and bonded-encryption support is
stored in NVS. Later scans accept only that BLE address. This prevents the
display from silently following another nearby MOT after initial setup. Because
the boards have no pairing input/output, BLE uses encrypted `Just Works`
bonding without MITM authentication; first pairing should therefore be done in
physical proximity to the intended adapter.

The status page advertises the initial three-second turn. The destructive step
is shown only after that hold and still requires the opposite confirmed hold.

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
  --version MOT-DISPLAY-1.1.0 \
  --output-dir /tmp/mot-display-1.1.0
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

The repository candidate uses PlatformIO's merged `firmware.factory.bin` for the
first installation on an untouched supported display. Package it only through
the contract checker; do not substitute an application-only image:

```sh
python3 tools/package_display_webflash_release.py \
  --version MOT-DISPLAY-PROTOTYPE-0.2 \
  --output-dir build/display-webflash
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
