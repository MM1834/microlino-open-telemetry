# ESP32-C6 firmware

This is the shared C6 firmware line established by C6-001 for the Muse Lab nanoESP32-C6-N16 and
Seeed XIAO ESP32-C6 board profiles.

Current implemented slice:

- board-specific PlatformIO environments and flash partitions;
- two concurrent ESP32-C6 TWAI controllers at 500 kbit/s;
- listen-only operation with independent counters and decoder profiles;
- persistent channel-specific CAN profile selection in Preferences/NVS;
- Display-CAN, Standard-CAN V1 - Pioneer / Gen1 Mid-Range and Standard-CAN V2 shared decoders;
- passive Pioneer VCU, IPC and BDU software-version decoding with local UI,
  AWS/local-MQTT and additive encrypted MOT Display publication;
- board, flash, CAN and GPS pin diagnostics;
- shared optional-GPS detection and NMEA fix-state handling;
- bounded in-memory drive capture with on-demand summaries for known CAN IDs;
- WiFi station configuration and optional runtime per-device AWS IoT publication;
- N16-only journey-energy accumulator with 60-second durable checkpoints and a
  separate acknowledged completed-journey replay journal under the existing
  offline-cache setting;
- protected device-specific setup/fallback AP and non-blocking reconnect;
- authenticated local WebUI, backup/restore, factory reset and local OTA;
- authenticated seven-step local device onboarding wizard;
- optional asynchronous WiFi ABRP service that can run alongside AWS IoT;
- XIAO internal ceramic antenna by default, with explicit external-U.FL build
  selection for hardware fitted with a 2.4 GHz antenna.

The maintained `xiao-esp32c6` environment selects the onboard ceramic antenna
and prints that selection at boot. For a dedicated unit
with a connected external antenna, add `MOT_XIAO_EXTERNAL_ANTENNA=1` to that
unit's build flags; do not distribute such an image to XIAO units without an
antenna on the U.FL connector.

Build without flashing:

```sh
pio run -e nanoesp32c6-n16
pio run -e xiao-esp32c6
```

The canonical `nanoesp32c6-n16` production environment includes the bounded
local-display BLE GATT service. It
publishes one versioned, CRC-protected 16-byte notification at no more than 5 Hz
and exposes no write characteristic. BLE remains disabled on XIAO. The former
`nanoesp32c6-n16-display-ble` environment remains as a configuration compatibility
alias for local tooling; both names produce the same production feature set and
firmware identity. Build the release artifact only with the canonical name:

```sh
pio run -e nanoesp32c6-n16
```

Both canonical environments always include AWS IoT and LittleFS. MOT Cloud is
enabled by default but can be disabled independently in the authenticated local
UI without deleting the valid credential set under `/aws`. Without credentials,
or with MOT Cloud disabled, local WebUI, CAN, GPS, OTA and ABRP continue normally.
There are no separate C6 non-AWS or `-aws` firmware generations.

GPS reception is physically validated separately on both boards. The N16 also
passed simultaneous dual-CAN reception and a normal-road WiFi/AWS IoT run with a
unique certificate and live portal ingestion. Local administration and OTA now
use the same security and OTA core as WROOM. The N16 AWS runtime and local-service
path are accepted; physical operation of the unified image without provisioned
AWS credentials remains the C6-ENV-001 rollout gate. Physical USB is the supported
recovery path and signed-image rollback is not claimed.

Default decoder assignment is Standard-CAN V1 - Pioneer / Gen1 Mid-Range on CAN1 and Display-CAN
on CAN2. The C6 configuration restricts CAN1 to Standard-CAN V1 or V2; Display-CAN
cannot be selected or restored for that physical input. CAN2 retains its separate
profile assignment and is fixed to Display-CAN during onboarding.

## Mobile test capture

The capture starts automatically from an empty state at every boot and records a
bounded summary in RAM without transmitting on either CAN bus. It tracks the
known Pioneer and large-battery Standard-CAN candidates (`0x18D`, `0x1B0`,
`0x1B1`, `0x2BA`, `0x37F`, `0x4AD`) and Display-CAN SOC, speed, power-display and
plug candidates. Use the serial console at 115200 baud:

```text
drive reset
drive dump
drive trace
```

`drive reset` starts a new observation window. `drive dump` prints the accumulated
minima, maxima, status-bit changes and representative raw frames. The result is
lost if the C6 is reset or loses USB power, so keep it powered until the dump has
been collected.

`drive trace` prints up to 600 synchronized samples at 200 ms intervals (the most
recent 120 seconds): speed, Standard-CAN current/voltage/status, derived power,
Display-CAN power byte and plausibility result. This is intended for the bounded
flat-road traction/regeneration test. The completed 2026-08-06 test confirmed
negative pack current during traction, positive pack current during regeneration
and increased electrical regeneration under light braking.

### Dedicated B.025 SOC/lock-discovery image

The `nanoesp32c6-n16-soc-diagnostic` environment adds a passive, RAM-only scan of
all 11-bit CAN1 identifiers plus a bounded change recorder for every 11-bit frame
on CAN1 and CAN2. It is intended for the B.025 test adapter and reports itself as
`C6-001-REV48-VCU-TEMP-DIAG-B025`; it does not change decoder or AWS topic
semantics. At a stable displayed SOC, run `soc reset`, wait for all buses to be
active, then run `soc mark`. After driving or charging changes the displayed SOC,
run `soc dump`. The comparison lists byte and 16-bit little-/big-endian fields
whose delta exactly matches 1, 2, 10 or 100 raw units per percentage point.
Repeating the process across several SOC transitions eliminates rolling counters
and other coincidental matches. State is lost on restart.
`soc diff` compares the latest payload of every observed 11-bit CAN1 identifier
with the last `soc mark`, even when Display-CAN SOC has not changed. It is intended
for controlled plugged and charging state transitions; repeat transitions to
separate state bits from rolling counters.

For lock-state discovery, keep the door position unchanged and run `lock start`
before the remote-control transition. `lock stop` freezes the 1,024 most recent
payload changes, `lock dump` prints them with channel and timestamp, and
`lock diff` compares the latest payload per identifier with the start mark.
Repeated lock or unlock commands without a state change help distinguish a
momentary command from a persistent locked/unlocked bit. The recorder is passive,
RAM-only and is cleared on restart or by `lock reset`. A single identifier is
limited to 32 stored changes per capture so a rolling counter cannot displace
the state transitions from all other identifiers.

The same B.025 image also includes a narrow passive VCU diagnostic recorder.
`vcu reset` starts a fresh RAM-only observation and `vcu dump` reports raw,
minimum, maximum and candidate-decoded values for Standard-CAN `0x105`
temperatures, Standard-CAN `0x700` VCU software version and Display-CAN `0x606`
VCU software version. The candidate versions are compared byte-for-byte when
both are present. No value is persisted or published, and both CAN controllers
remain in listen-only mode with zero transmit queue.
`version search` scans the latest already-received payloads in RAM for the
IPC `1.3.0` and BDU `1.1.0` layouts and prints matching channel, identifier and
raw bytes. It does not transmit a CAN frame.
The diagnostic image disables the optional adapter-to-MOT-Display BLE server so
the RAM-heavy SOC/lock recorder cannot compete with the BLE controller. CAN1,
CAN2, USB serial diagnostics and the local WebUI remain available. AWS IoT is
also excluded from this temporary image so TLS retries cannot consume the RAM
reserved for the bounded capture; stored credentials are neither read nor changed.

### Dedicated B.025 data-usage analysis image

The `nanoesp32c6-n16-data-analysis-b025` environment uses the current REV36 N16
feature set without the RAM-heavy SOC/lock recorder and identifies itself as
`C6-001-REV36-DATA-ANALYSIS2-B025`. Runtime diagnostics expose separate AWS and
secure-local-MQTT publish counts, application payload bytes, estimated MQTT
PUBLISH packet bytes, connection successes or reconnects, and disconnects.
This second temporary analysis image checkpoints the three local-MQTT volume
counters as one NVS blob at most every 20 minutes and only after a successful
publish changed them. A restart restores the last completed checkpoint, so a
hard power loss can discard at most the newest 20-minute interval. Additional
status fields expose restore state, successful checkpoints, write failures and
checkpoint age. Production builds remain RAM-only. All variants deliberately
exclude TLS, TCP/IP and mobile-network framing. Compare their deltas over the
same interval with the router/provider counter; a large remainder indicates
connection handshakes, other router traffic or accounting outside MQTT PUBLISH
packets.

### Optional N16 BM2 BLE diagnostic image

The `nanoesp32c6-n16-bm2-diagnostic` environment is a separate BM2/BM6 diagnostic
application based on the complete REV36 N16 firmware. It identifies itself as
`C6-001-REV36-BM2-BM6-AUTO-DIAG`, uses the normal 16 MB partition table and does not erase
or migrate NVS, WiFi, service credentials or local configuration. It is not a
production or WebFlash target and is deliberately unavailable for the 4 MB XIAO.

The image performs bounded BLE identification scans after the normal network startup.
It first checks for the 16-byte encrypted manufacturer advertisement used by some
BM2-type 12-V battery monitors. Active scan requests also retrieve device names
and advertised services from otherwise incompatible candidates. Automatic scans
start after 30 seconds, last 15 seconds and repeat at most once per minute with a
20-percent scan window to keep initial WiFi/AWS qualification representative.

After a qualified candidate is found, this special diagnostic image automatically
opens one bounded GATT session. BM6 receives only the documented live activation
query. BM2 first collects live FFF4 values and then requests the known read-only
history transfer. The session ends after a successful capture or after 60 seconds
and permits no more than three connection attempts per boot. The physically tested
BM2 accepts only one BLE client at a time, so its smartphone application must be
fully disconnected during diagnosis.

The authenticated local Runtime Diagnostics expose the phase, attempt count,
single-client warning, live values and BM2 history summary under
`bm2.autoDiagnostic`; a remote pilot needs no terminal command. The USB console
commands remain available for development:

```text
bm2 scan
bm2 stop
bm2 status
bm2 connect
bm6 connect
bm2 disconnect
bm2 history
bm2 history dump
```

The exact AEG BM2 was identified as an FFF0 device whose advertisement does not
contain the known live-value frame. FFF4 voltage notifications are physically
confirmed. The explicit `bm2 history` command sends only the documented encrypted
history count/data requests to FFF3; it does not change configuration. A bounded
8-KiB buffer summarizes up to 2,048 two-minute samples, and `bm2 history dump` emits the
reconstructed UTC series for analysis. No AWS publication or cloud-portal
integration exists. BM2 telemetry remains outside numbered production
firmware, and all other writes remain outside this diagnostic slice.

For a scanned device named `BM6`, `bm6 connect` uses its separate AES key,
subscribes to FFF4 and sends the documented encrypted `D1 55 07` activation
query through FFF3. The diagnostic decoder exposes plausible voltage,
temperature and SOC. BM6 History is not implemented because the available
reference protocol documents only the live response. BM2 and BM6 state remain
strictly separate.

## WiFi and AWS qualification

For first setup, connect to the WPA2-protected `MOT-XXXXXX` AP. Its device-specific
password is printed on USB serial. Open `http://192.168.4.1/`, authenticate as
`setup`, and set a unique 12–63 character local administrator password.
If native USB reconnects after the boot message, enter `setup status` on the
115200-baud serial console. It reveals the setup credential only before a local
administrator password exists.

If that administrator password is lost, connect physically over USB and enter
`admin recover`. The device replaces only the administrator password with a new
random password and prints it once on that serial console. WiFi and decoder
settings remain unchanged.

The same physical USB recovery command is available on the ESP32-WROOM and
LilyGO T-A7670 firmware families.

WiFi can also be configured over serial without echoing the password:

```text
wifi set MyHomeSSID|MyPassword
wifi2 set MyMobileHotspot|MyPassword
wifi status
aws status
aws enable
aws disable
```

`wifi clear` and `wifi2 clear` remove each profile independently. Restart after
changing WiFi through the console. The canonical builds load unique X.509
credential files from LittleFS `/aws`; credentials are not linked into the
application image. The shared upload helper accepts `esp32-c6` and defaults to
`nanoesp32c6-n16`, with `xiao-esp32c6` selectable explicitly.

`aws disable` stops MOT Cloud while retaining the adapter's X.509 credentials and
vehicle assignment; `aws enable` restores the saved service state after restart.
The same switch is available in `/wizard` and `/config`. ABRP remains independently
selectable and can be the only enabled Internet telemetry service.

Blank ABRP credential fields intentionally keep the current secret values. Use
the separately confirmed `Delete ABRP credentials` button, or `abrp clear` on the
physical USB console, to disable ABRP and remove both values from NVS.

The C6 publisher sends Display-CAN, confirmed Pioneer pack voltage/current/power,
plug/charge state, provisional cell extrema and valid GPS coordinates using the
same topic contract as the established devices.

On `nanoesp32c6-n16`, REV11 additionally integrates fresh Standard-CAN vehicle
power while moving and publishes non-decreasing drawn/regenerated Wh counters.
The counter performs no flash writes and is intentionally disabled on XIAO.
REV17 treats a first zero-speed sample only as a stop candidate, seals after ten
continuous stopped minutes, and seals immediately on fresh Standard-CAN plug or
charging state. A sealed final counter is published before a later journey may
replace its identity.
`energy status` prints its current RAM state over the USB serial console. Missing
or incomplete firmware evidence remains compatible with the backend telemetry
estimate.

AWS status includes the MQTT result, bounded connect duration and the most recent
TLS error code/detail. This distinguishes missing credentials from DNS/TCP/TLS
failure without exposing certificate or key material.

## ABRP and local onboarding

ABRP is disabled by default. Configure and enable it under `/config`; its API key
and user token are not rendered back into HTML, diagnostics or normal backup
exports. `/api/abrp/test` queues an HTTPS send in a separate task so Dual-CAN and
GPS processing continue. The power field uses fresh Standard-CAN vehicle power
in kW when available and falls back to the Display-CAN 0.1 kW value.

Freshly provisioned devices enter `/wizard`. Existing configured C6 installations
without an onboarding key migrate as completed and therefore do not unexpectedly
enter the wizard after OTA. The wizard configures only the local adapter; portal
accounts, vehicle ownership and certificate assignment remain external admin work.

First setup now performs only the one-time transition from the device-specific
`setup` credential to the user-selected `admin` credential. WiFi, CAN and optional
services are edited inside the wizard without intermediate restarts. A review page
shows the future SSIDs and non-secret settings, then applies hardware, WiFi, CAN and
services with one consolidated restart. The real router-assigned IP and runtime
validation are shown only after that connection attempt and restart. Wizard progress
persists throughout the flow.
Until explicit completion, the protected `MOT-*` AP remains active at
`192.168.4.1` alongside a successful station connection. The final page shows the
active Home or Mobile/WiFi2 SSID and current station IP when connected, plus the
fallback-AP route without rendering stored passwords. After completion, a stable
station connection stops the AP; loss of both configured networks restores it.

For the 4 MB XIAO, run `python3 tools/check_c6_flash_gate.py` after building
`xiao-esp32c6`. The maintained gate is 85% of one OTA application slot.
