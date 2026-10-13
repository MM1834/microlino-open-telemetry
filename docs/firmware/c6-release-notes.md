# ESP32-C6 Firmware Release Notes

> **Current repository release:** `C6-001-REV41-AWS`

> **Separate analysis build:** `C6-001-REV36-BM2-BM6-DIAG` is an N16-only,
> BLE battery-monitor qualification image for separate BM2 and BM6 protocols. It does not change the production REV36
> release or its WebFlash artifacts.
>
> **Current field-accepted release:** `C6-001-REV29-AWS`

### `C6-001-REV41-AWS` — repository candidate

REV41 extends the encrypted local Display-CAN emergency-instrument record with
four `0x603` states: parking brake at Intel start bit 20, warning byte 3,
HBL/AH warning at bit 43 and yellow drivetrain warning at
bit 45. Display firmware `MOT-DISPLAY-1.1.0` renders a red `(P)`-style symbol
and replaces the lower informational lines with active yellow warning text.

This is a deliberate controlled break of the 12-byte Display 1.0 driving
record: the paired record is now 14 bytes. Both known adapters and both known
1.28-inch displays must therefore be updated together. The four states remain
local to the adapter/display BLE link and are absent from AWS and local-MQTT
payloads. Seventy-nine focused contracts and both N16/display builds pass.
The reviewed configuration-preserving N16 OTA application is 1,926,000 bytes with SHA-256
`6ddd205feb731650191f9818712762838f445771718af70743d078491667c59d`.
The Display 1.1.0 application is 856,224 bytes with SHA-256
`1e392f648ab8897fb9a577ecac2fa6a0e9a6ae310719a14648607866791dacab`.
The XIAO regression application is 1,493,808 bytes with SHA-256
`99d583b82a4ebf5d3d7b553d77366fbe37be62866ba31ce454c0785935e311f5`;
it remains without Display BLE.
The USB-connected N16 adapter and 1.28-inch display were updated together and
verified by the flash loader. The display layout marker was restored after the
USB upload and read back with its expected SHA-256. Display WebFlash factory and
marker-gated update catalogs are active on 1.1.0 through the replacement-free
Change Set `display-webflash-1-1-0-20261013`; the normal N16 WebFlash catalog
deliberately remains REV38. The controlled pair is installed and boot-verified;
the final vehicle warning-signal spot check is part of the pre-shipment
operational acceptance.

### `C6-001-REV40-AWS` — repository production candidate

REV40 promotes the accepted local Display BLE contract into the canonical N16
release identity. It adds the optional driving-status record with Display-CAN
speed and locally decoded left/right/hazard state. Indicator state is used only
by the encrypted local display service and is absent from AWS and local-MQTT
payload construction. The former `nanoesp32c6-n16-display-ble` environment is a
configuration compatibility alias for `nanoesp32c6-n16`, not a separate firmware stream.
The XIAO build remains without Display BLE.

The configuration-preserving N16 application is 1,925,648 bytes with SHA-256
`45e71f144abca73623398199637f9aa610ae8942fbf71c544bb125c676919e4c`.
The XIAO compatibility application is 1,493,696 bytes with SHA-256
`a2d358a2e6582ee64e602058fdec6d0bd8834b727f06f804fc56df1a98040971`.
Both builds, the compatibility alias, the Display 1.0.0 build and 68 focused
contracts pass. Catalog activation and physical installation remain separately
controlled release operations.

### `C6-001-REV39-AWS` — repository production candidate

REV39 promotes the physically validated PRE7 N16 service-coexistence work into
the canonical production build. `nanoesp32c6-n16` now includes the bonded,
read-only MOT Display BLE service and the bounded TLS ownership handoff between
AWS IoT, secure local MQTT and ABRP. Endpoint resolution remains on the Arduino
loop task; blocking cloud work remains on the dedicated worker. AWS yields only
after a stable, recently productive connection, reconnects after ABRP or local
MQTT releases TLS, and disabled ABRP cannot request a pause.

The exact N16 application is 1,924,800 bytes with SHA-256
`25de26cfff41fd0de3cb8e98a9163d4cc2513cca771aef5420727f3ccbd5c9f0`.
The XIAO compatibility application remains without Display BLE and is 1,493,488
bytes with SHA-256
`d41f76b3bc6eecad41d4eb79a675fac68239b4cef81717901d4f804f93bcb5ac`.
Both builds and 112 focused firmware/service/policy/backfill tests pass. Physical
PRE7 evidence covers the N16 implementation: ABRP returned HTTP 200, the local
MQTT burst completed and AWS reconnected stably after its policy correction.
REV38 remains the active controlled WebFlash release and rollback point; no
catalog object, grant or deployed firmware was changed by this promotion.

### `C6-001-REV38-AWS-DISPLAY-BLE-PRE7` — physically validated recovery candidate

PRE7 completes the display-build TLS ownership work started in PRE5/PRE6. AWS
now yields deliberately to bounded ABRP and secure-local-MQTT TLS windows and
reconnects after their TLS objects are released. On the physical Pioneer N16,
ABRP returned HTTP 200, the local-MQTT burst completed, AWS reconnected with
zero current failures, and free heap recovered to 118,364 bytes with an 88,052-
byte largest block. The connected MOT Display was previously measured at only
about 2.4 KiB additional heap and is not the cause of the original ABRP deferral.

The remaining AWS MQTT state `-3` was independently traced with an adapter-only
AWS IoT debug target to a missing authorization for
`mot/pioneer/charge/backfill/ack/v1`. Policy version 4 adds only
`iot:Subscribe` on that exact topic filter and `iot:Receive` on that exact topic.
After propagation, AWS IoT recorded successful subscriptions for charge,
journey and history acknowledgements, followed by successful heartbeat,
telemetry and MQTT ping traffic. The provisioning script and its contract test
now include all three acknowledgement paths so future device policies retain the
fix. Temporary adapter-specific debug logging and its IAM role were removed;
the diagnostic log group expires under its 30-day retention setting.

The exact PRE7 application is 1,924,848 bytes with SHA-256
`fc750606b085227691aa60ef7d4c43077efef713cd9c03ff03304db1b49b5d1f`.
It is installed on the Pioneer qualification adapter. Forty-seven focused AWS
policy/service-parity/local-MQTT tests pass. PRE7 remains a local candidate; no
controlled WebFlash release was changed. A follow-up account-wide audit then
added the same vehicle-bound Charge-ACK Subscribe/Receive pair to the other 25
regular bidirectional MOT device policies. Read-back confirms the exact pair on
all 26 such policies. The publish-only Node-RED and WiCAN Lite identities plus
the generic historical test policy remain unchanged.

### `C6-001-REV38-AWS-DISPLAY-BLE-PRE6` — local recovery candidate

PRE6 fixes the ABRP pause-state regression isolated during the 2026-10-12 PRE5
physical soak. In the display build, an elapsed ABRP cadence could request and
immediately release the cloud-worker TLS pause even while ABRP was disabled.
That repeatedly closed an otherwise successful AWS MQTT connection after about
100–300 ms (`MQTT_CONNECTION_LOST`, state `-3`). AWS-only testing reproduced
the failure with local MQTT disabled and ABRP configured off, while the serial
log exposed the repeated ABRP pause/release cycle.

The C6 ABRP adapter now exits before network preparation or pause arbitration
when ABRP is disabled or its credentials are incomplete. It also cancels a
pending display-build request exactly once if ABRP is disabled during a send.
The regression test asserts that this enabled check precedes every pause
request. The PRE6 display application builds at 1,923,760 bytes with SHA-256
`bc310e06643881a10503edbae0ce23d6ef64d4651ec8b7eec197bdd72d4c0768`;
all 68 focused display/service/MQTT/cadence tests pass. Physical AWS-only and
combined-service acceptance remain required before promotion.

### `C6-001-REV38-AWS-DISPLAY-BLE-PRE5` — local recovery candidate

PRE5 corrects the cloud-task and ABRP regression found during the 2026-10-12
Pioneer USB diagnosis. The failing PRE4 build invoked Arduino's hostname
resolver from the separate cloud worker; the decoded panic ended in lwIP
`udp_new_ip_type` with `Required to lock TCPIP core functionality`. PRE5 resolves
AWS, secure local MQTT and ABRP endpoints on the Arduino loop task, then gives
the worker a resolved-address TLS client that retains the original hostname for
SNI and certificate verification.

The display build also treats AWS, secure local MQTT and ABRP as serialized TLS
owners. Before an ABRP HTTPS task starts, the cloud worker closes both persistent
MQTT transports and acknowledges the pause. It reconnects them only after the
ABRP task has destroyed its HTTP/TLS objects. This retains the independent
service settings and credentials while preventing the measured overlapping-heap
condition. The canonical non-display N16 path keeps its established scheduling.

Live evidence preceding the correction showed that the connected display itself
cost only about 2.4 KiB. With local MQTT removed, ABRP immediately returned HTTP
200 with zero low-memory skips; the separate worker DNS call then reproduced a
panic reset. The PRE5 repository candidate builds at 1,923,696 bytes with SHA-256
`701b4dfa2991373d65bef4e22554021046578d138321898e5fd253bf00c608fd`.
The canonical N16 regression build also passes at 1,526,496 bytes. Sixty-eight
focused display/service/MQTT/cadence tests pass. Physical installation and a
combined AWS/local-MQTT/ABRP soak remain open; PRE5 is not a WebFlash release.

### `C6-001-REV38-AWS` — active N16 WebFlash release

REV38 adds the bounded N16 offline-charge journal from OFFLINE-ACT-001. A
completed charging session that experienced a connectivity gap is retained in
LittleFS and replayed as `charge/backfill/v1`; it is removed only after the
matching AWS acknowledgement. The journal holds at most 16 compact records and
is covered by the existing offline-cache setting and purge operation. AWS also
reconciles fully offline charging from reconnect SOC and odometer snapshots.

The initial release scope is `nanoesp32c6-n16`. XIAO and all other hardware
remain outside this REV38 release until separately approved. Physical smoke and
offline-charge replay acceptance remain open. The exact configuration-preserving
N16 application is 1,523,824 bytes with SHA-256
`c40a77ba5fc34c2425326dc500f6c3202104fd9bd90b0cfa117929afecb9eacf`.
Private AES256/versioned S3 read-back matched on object version
`f7wSa87gZALHAWwCPXJuNI3ezAsY6.8d`. Replacement-free Change Set
`webflash-rev38-20261004` updated only the existing Onboarding Lambda and its
exact-object IAM policy; the stack returned to `UPDATE_COMPLETE`. XIAO remains
on REV36. Existing REV37 grants were deliberately not migrated and therefore
fail closed until REV38 access is separately approved.

### `C6-001-REV37-AWS` — repository candidate

REV37 adds the N16 local journey recuperation peak from JNY-REGEN-PEAK-001. It
updates the peak only from fresh valid Standard-CAN vehicle power while movement
is confirmed and no plug/charging boundary is active. The non-decreasing whole-
watt value joins the existing journey checkpoint and bounded offline journal;
AWS prefers it over the lower-resolution observed telemetry peak for the same
journey. No high-frequency history is added. XIAO retains the shared firmware
line but its N16-only journey counter remains disabled.

The exact configuration-preserving N16 WebFlash application is 1,516,064 bytes
with SHA-256
`53e50d2aabf4c6c93467e73814641ec0be3e313a856a3f2b1dd59737da7eca0d`.
Private AES256/versioned S3 read-back matched on object version
`EankjqU6MxPeo.DkSM93fePAFqBzz0yM`. Reviewed replacement-free Change Set
`webflash-rev37-20261001` updated only the existing Lambda, its exact-object IAM
policy and dynamically referenced API integration; the stack returned to
`UPDATE_COMPLETE`. XIAO remains on REV36. Exactly four previously active N16
grants were atomically migrated from the REV36 identity to REV37 with their
existing expiries preserved and four audit events. All four read-only access
checks return the exact REV37 release, anonymous HTTP remains 401 and the recent
Lambda error log is clean. Physical installation remains open.

### `C6-001-REV36-AWS` — repository candidate

REV36 connects direct ABRP to the same adaptive scheduler introduced for AWS
and local MQTT. ABRP keeps its independent HTTPS/JSON protocol and 60-second
active base interval, but uses the 5-/10-minute parked steps and immediately
queues on scheduler events. The controlled Nano test of REV35 remains valid as
the MQTT-envelope/cadence baseline; REV36 adds only this ABRP scheduling slice.

The exact REV36 application artifacts are active in the controlled WebFlash
catalog for both `nanoesp32c6-n16` and `xiao-esp32c6` through replacement-free
Change Set `webflash-rev36-20260928`. N16 is 1,506,752 bytes with SHA-256
`b067438d46a8e4502065f22be25733bb57fbb90911f1876a4e8fb65f04bab71a`;
XIAO is 1,481,264 bytes with SHA-256
`aa8718e33763972c16282dfd401e26e28e7d1525237c89a9d636e638c0234ca3`.
Both S3 read-backs matched, the objects are AES256 encrypted and versioned, the
stack and Lambda are healthy, and anonymous firmware access remains HTTP 401.
Physical WebFlash installation of this exact release remains open.

### `C6-001-REV35-AWS` — repository candidate

REV35 extends `telemetry/state/v1` to secure local MQTT with no local scalar or
dual-publish mode. AWS and local MQTT now use independent configurable active
intervals (defaults 5 s and 60 s) plus one shared adaptive cadence: 30 s minimum
while charging, 5 minutes after 10 minutes parked and 10 minutes after 30
minutes parked. Motion, charge, plug, door, electronic-lock and entry into the
provisional absolute 12 kW peak band publish immediately; reconnect also sends
immediately. The first local connection removes retained telemetry scalars left
by older firmware and records completion. An envelope-aware ioBroker consumer
and physical cadence/migration testing remain release gates.

### `C6-001-REV34-AWS` — controlled AWS-envelope milestone

REV34 replaces the periodic C6 AWS scalar burst with one retained, versioned,
sub-4096-byte `telemetry/state/v1` envelope. AWS accepts both the new exact topic
and legacy scalar topics automatically and normalizes them behind the unchanged
State, History, WebSocket, notification and Vehicle API contracts. REV34 does
not dual-publish periodic telemetry; rollback uses the previous firmware. N16
and XIAO builds pass at 27.4%/18.2% and 83.0%/17.9% Flash/RAM respectively.
Backend deployment and synthetic productive-IoT coexistence acceptance passed
on 2026-09-26 with encrypted rollback artifacts. Controlled physical REV34
publication, reconnect and roaming measurement remain open.

### `C6-001-REV33-AWS` — repository candidate

REV33 starts JNY-OFFLINE-001. On N16, the existing offline-cache setting now
controls both the unchanged SOC/Speed History journal and a separate checksummed,
bounded completed-journey journal. A journey that experienced an Internet gap is
checkpointed, survives restart and is replayed on its own versioned MQTT path;
local deletion follows only a matching AWS acknowledgement. The compact record
contains UTC, odometer/SOC endpoints and energy totals but no GPS, route, WiFi or
IP data. The AWS repository path is fail-closed behind a global default-off
switch and has no vehicle allowlist. N16 builds at 27.4% flash and 18.1% RAM;
automated backend, isolation and firmware-contract tests pass. Physical outage,
reconnect and restart acceptance remains open.

### `C6-001-REV32-AWS` — repository candidate

REV32 adds the decoder-valid BMS SOC and cell-voltage values to C6 secure local
MQTT, matching the existing AWS topic contract. This includes the Pioneer V1
internal and display-correlated SOC candidates, V2 Standard-CAN SOC, pack
voltage and the provisional cell-voltage extrema and delta. Seventeen focused
BMS/local-MQTT tests pass. C6 N16 builds at 27.2% flash and 18.1% RAM; XIAO
compatibility builds at 82.8% flash and 17.9% RAM. The functionality was
physically accepted from the preceding build on 2026-09-21; installation and
confirmation of the correctly identified REV32 artifact remain open.

### `C6-001-REV31-AWS` — repository candidate

REV31 adds local altitude-filter diagnostics without publishing rejected data.
Runtime Diagnostics and serial output now show raw GNSS height/validity, the
current acceptance decision and reason, filter state, rejection count and last
accepted moving height. MQTT/AWS qualification remains unchanged. The exact N16
WebFlash application is 1,484,336 bytes with SHA-256
`3df6a9148bde70a7d1088e4fe44d3e35b501042319b95b44edd9d7585ad4a77a`.
S3 read-back reproduced that hash and confirmed AES256 encryption on object
version `u0YCc22.0cFUJZqxtVrpxPMRWPQHK0z8`. Reviewed Change Set
`webflash-rev31-20260918` updated only the existing Lambda, integration and IAM
role with `Replacement: False`; the stack is `UPDATE_COMPLETE` and Lambda is
`Active`/`Successful`. The four still-active N16 grants were atomically updated
to the exact REV31 identity with four audit events; the separately revoked grant
remains untouched. Anonymous firmware access returns HTTP 401. Local OTA and
diagnostic-field acceptance passed; physical WebFlash installation remains open.

### `C6-001-REV30-AWS` — repository candidate

REV30 discards Display-CAN `0x604 data[4] bit 4` as a plugged-state source on
every board. An unplugged B.031 live test reported the bit as set, disproving the
former interpretation. Standard-CAN V1/V2 remain the only authoritative plugged
sources. C6 drive diagnostics retain the raw bit solely as a clearly labelled
discarded candidate. C6 N16, XIAO and LilyGO A7670 AWS builds pass; physical
installation and release publication remain open.

### `C6-001-REV29-AWS`

REV29 makes the N16 onboard status LED unambiguous for remote support. An
incomplete onboarding state breathes white; configured WiFi that is currently
offline breathes magenta instead of the former yellow that multiple observers
perceived as green. Solid blue continues to mean WiFi without a connected cloud
service, solid green means AWS IoT connected, cyan means local MQTT only, green
with a blue component means AWS plus local MQTT, and breathing red remains the
Dual-CAN startup-error indication. XIAO remains excluded from the GPIO8 LED path.
Focused contracts pass, and N16 plus XIAO builds pass; XIAO uses 82.5 percent of
its application slot. The exact N16 WebFlash candidate is 1,479,632 bytes with
SHA-256 `3879d337c88eb18c6b958ede018ebdf4894bc40045ee901e3e1a5a2d505b98a4`.
Its private AES256-encrypted S3 object version is
`q4WkhrOFo22Br9lsIaNOmNTzOXnZSdNa`.
Reviewed Change Set `webflash-rev29-20260917` modified only the existing Lambda,
API integration and IAM role with `Replacement: False`; the stack returned to
`UPDATE_COMPLETE` and the Lambda is `Active`/`Successful`. The four previously
valid N16 users have audited 48-hour grants for the exact REV29 version/hash; the
older independently revoked grant remains revoked. Anonymous access remains HTTP
401. Physical REV29 acceptance passed on 2026-09-17: USB diagnosis left the
adapter stable, USB selection kept it online until the explicit flash start, the
complete write restarted it into normal network operation, and the white
incomplete-onboarding plus magenta WiFi-offline LED indications were confirmed.

### `C6-001-REV28-AWS`

REV28 adds backward-compatible, read-only USB support diagnostics. The new
`support status` command reports firmware/device identity, uptime and reset
reason, heap watermarks, WiFi link and disconnect state, AWS/TLS status,
offline-cache counters, clock validity and CAN1/CAN2 frame health without
exposing credentials. `wifi status` additionally identifies the configured home
and mobile SSIDs after serial-safe normalization. The portal continues to issue
the established `profiles`, `wifi status`, `aws status` and `cache status`
commands after the new command, so adapters still running older firmware remain
diagnosable during rollout. Repository builds and automated contracts pass.
Physical diagnosis acceptance passed without resetting the adapter. Two complete
WebFlash recovery runs passed with the explicit DTR/RTS reset pulse returning the
adapter to normal operation. The exact 1,479,584-byte N16 application with
SHA-256 `1f7e1432ba154cbccf0f0ddfab110034ddb72169395886d1247979e20c933dd4`
is active in the controlled WebFlash catalog through replacement-free Change Set
`webflash-rev28-20260917`. Its private AES256-encrypted S3 object version is
`1fsXBRMpBZCqLif4PY6TIc_5M3mLMfCV`. The four previously valid N16 users were
granted the exact REV28 release for 48 hours; an older independently revoked
grant remains revoked.

### `C6-001-REV27-AWS`

REV27 adds the field-derived Display-CAN `0x601` access decoder. Byte 3 reports
the electronic central-lock state (`0x01` locked, `0x02` unlocked), while bit 7
of byte 0 reports the observed door-open state. Because tests showed that the
lock byte can remain stale during charging and after unplugging, firmware emits
a separate validity topic and requires a later electronic transition before the
lock state becomes valid again. Mechanical key operation remains outside the
confirmed signal boundary. C6 N16 and both maintained LilyGO AWS builds pass;
Physical firmware, AWS publication, the charging-uncertain presentation and live
language switching are field-accepted. The exact 1,477,344-byte N16 application
with SHA-256
`993b32c65904db00eda65166c58cea9577355baebde693db89ef9e5dde0bee4c` is active
in the controlled WebFlash catalog through replacement-free Change Set
`webflash-rev27-20260916`. Its private AES256-encrypted S3 object version is
`TBtGb9yKfGEiNncJIfPETi4O9ibPsQ.w`. The three existing N16 pilot grants were
atomically renewed for REV27 with unchanged expirations and successful audit
events. ACCESS-001 is complete.

### `C6-001-REV26-AWS`

REV26 closes the cold-start exception found when leaving the garage on REV25.
The first moving altitude after boot is now provisional and never published. A
second point at least 10 m away must pass the same distance-relative height check
before becoming the accepted anchor. Failed qualification restarts from the
newer provisional point without sending it, so a degraded initial fix cannot
poison the filter. The parallel C6 status-LED work remains intact.
Physical N16 acceptance passed on 2026-09-16: a short drive established valid
height, and subsequent waiting inside the garage produced no drifting replacement
value. The onboard GPIO8 RGB LED was also accepted on the physical N16 for boot,
WiFi, AWS IoT and local MQTT states. Its red Dual-CAN startup-error indication is
covered by source/build contracts but was not destructively provoked during the
field test.

The exact N16 application from commit `b427650` was activated in the controlled
WebFlash catalog through replacement-free Change Set
`webflash-rev26-20260916`. The private AES256-encrypted, versioned object is
1,476,224 bytes with SHA-256
`597c00565204d0531ec8b46d676071f3dc4dab1b4949a6ebe98fdcfaed0882d0`.
All three previously active N16 pilot grants were explicitly and atomically
renewed for the REV26 identity while retaining their existing expirations; each
renewal has a successful audit record. The XIAO catalog entry remains on REV21.

### `C6-001-REV25-AWS`

REV25 addresses the remaining height drop while rolling into the garage. It
requires at least six satellites and retains REV24's CAN-motion gate. After the
first moving anchor, height is published only after at least 10 m horizontal
movement and when the height change remains within 10 m plus 50 percent of that
distance. Rejected measurements do not advance the anchor, preventing degraded
indoor fixes from ratcheting the accepted height downward.

### `C6-001-REV24-AWS`

REV24 increases the socket inactivity timeout only for an authenticated,
same-origin local OTA upload from the ESP32 framework default of 5 seconds to
30 seconds. This tolerates transient packet loss and retransmission on weak WiFi
without changing Home/Mobile/AP selection, ordinary WebUI timeouts or image
validation. An actual connection loss still requires restarting the upload.

### `C6-001-REV23-AWS`

REV23 corrects an over-restrictive REV22 altitude gate. The first REV22 road test
recorded Display-CAN speeds up to 38 km/h and later 12 satellites but published
no height. REV23 keeps the decisive fresh CAN-motion requirement that suppresses
stationary garage drift, while restoring REV21's proven four-satellite and HDOP
5.0 GNSS thresholds. Invalid or stationary height remains omitted without a
separate validity topic.

### `C6-001-REV22-AWS`

REV22 closes the remaining indoor-altitude path observed with REV21. Satellite
count and HDOP must now be fresh as well as position and altitude, the height
gate requires at least six satellites and HDOP no greater than 2.5, and C6 plus
LilyGO publish height only with fresh Display-CAN motion above 1 km/h. Invalid or
stationary height is omitted; no separate validity topic is introduced.
The focused altitude contract and the N16, XIAO, T-A7670X and
T-SIM7670G-S3 AWS build gates pass. Physical road-to-garage acceptance remains
open.

### `C6-001-REV21-AWS`

REV21 hardens the REV20 altitude publisher after an indoor field observation.
A general fresh location fix is no longer sufficient for height: location and
altitude must both be at most five seconds old, at least four satellites must be
reported and HDOP must be no greater than 5.0. When that separate quality gate
fails, C6 and LilyGO omit `location/altitude_m` instead of publishing a drifting
indoor value. GPS diagnostics expose `altitudeValid` and `altitudeAgeMs`.

The immutable N16 and XIAO application images are active in the controlled
WebFlash catalog. Change Set `webflash-rev21-20260915` updated only the existing
Lambda environment, its exact S3 object permissions and the dependent API
integration, all without replacement. The stack returned to `UPDATE_COMPLETE`;
the Lambda is `Active`/`Successful` and both private artifacts are AES256
encrypted and versioned. Existing user grants remain release-bound and therefore
must be explicitly issued or renewed for REV21.

### `C6-001-REV20-AWS`

REV20 publishes raw valid-fix GPS altitude as `location/altitude_m` through MOT
Cloud/AWS and the optional secure local MQTT service. Physical C6 testing
confirmed plausible outdoor altitude on both transports. The N16 and XIAO build
gates remain part of the release validation; WROOM remains outside this release
boundary because its existing image exceeds its OTA slot.

REV20 includes the now accepted REV19 Standard-CAN V2 plug and charging-state
decoder described below.

### `C6-001-REV19-AWS`

REV19 separates the newly verified Standard-CAN V2 charging-state decoder from
the previously accepted REV18 release identity. On the tested newer-generation
large-battery vehicle, Standard-CAN `0x60D` byte 1 bit 3 identifies a connected
charge cable, while `0x610` byte 0 distinguishes unplugged (`0x01`), plugged and
idle (`0x05`) and actively charging (`0x02`). Pack voltage, current and derived
power remained plausible during controlled 6 A and 8 A charging tests, and both
CAN channels remained error-free.

The plug and active-charging portion is field-accepted after the original
large-battery test and a subsequent V2 confirmation. This does not claim that
traction and regeneration have been validated on every newer-generation
battery/BMS variant.
>
> **Targets:** nanoESP32-C6-N16 and Seeed XIAO ESP32-C6

This page summarizes the operational C6 revision sequence beginning with the
first XIAO-capable `C6-001-AWS` pilot. Some intermediate field revisions were
built and validated between consolidated repository commits; the entries below
therefore describe their functional boundary rather than claiming one Git commit
per revision.

## Revision history

### `C6-001-AWS` / `C6-001-REV1-AWS`

- first shared N16/XIAO ESP32-C6 firmware line;
- two passive 500 kbit/s TWAI/CAN inputs with independent decoder profiles;
- Display-CAN, Pioneer Standard-CAN V1 and provisional V2 profiles;
- WiFi/AWS telemetry, GPS, CAN scan and bounded drive capture;
- separate 16 MB N16 and 4 MB XIAO partition/pin profiles.

### `C6-001-REV2-AWS`

- protected setup/fallback access point and authenticated local WebUI;
- configuration backup/restore and factory reset;
- local OTA, failed-image recovery and physical USB administrator recovery;
- cooperative WiFi/CAN/GPS/AWS runtime hardening.

### `C6-001-REV3-AWS`

- expanded WiFi/AWS/TLS, reset, heap and disconnect diagnostics;
- weak-link and mesh/BSSID observation support;
- diagnostic field revision; no intended telemetry-contract break.

### `C6-001-REV4-AWS`

- ABRP TLS object lifetime and heap-leak correction;
- ABRP memory gates and asynchronous/manual status controls;
- associated Home WiFi is no longer abandoned only because of weak RSSI;
- bounded Home/Mobile/AP recovery retained.

### `C6-001-REV5-AWS`

- restored unconditional Home-WiFi priority when the configured Home SSID is
  visible;
- automatic Mobile-to-Home return without depending on an RSSI threshold;
- AWS, ABRP, Dual-CAN and GPS coexistence field-validated on N16.

### `C6-001-REV6-AWS`

- persistent GPS enable/disable control in Configuration and onboarding;
- explicit `GPS_DISABLED` diagnostics and backup/restore support;
- service and Standard-CAN plausibility hardening.

### `C6-001-REV7-AWS`

- receive-only/listen-only CAN contract aligned across maintained adapters;
- optional offline SOC/speed History cache and acknowledged AWS backfill;
- N16 cache limit 256 KiB, XIAO cache limit 128 KiB;
- no GPS/location storage in the offline cache.

### `C6-001-REV8-AWS`

- Standard-CAN V1/V2 becomes authoritative for plug and charging state;
- Display-CAN charging frames `0x603` and `0x604` are ignored whenever a
  Standard-CAN profile is configured;
- prevents false journey completion from conflicting Display-CAN charge data.

### `C6-001-REV9-AWS`

- guided local onboarding refined into a persistent seven-step flow;
- first setup separated from WiFi, CAN and optional-service configuration;
- protected device AP retained until onboarding completes;
- configuration-preserving migration for already deployed adapters.

### `C6-001-REV10-AWS`

- Standard-CAN V2 decoding expanded for pack voltage, cell extrema, SOC, SOH,
  current and vehicle power;
- MOT Cloud and ABRP can be controlled independently without deleting AWS
  credentials;
- ABRP credential deletion and additional AWS/TLS diagnostics;
- charging/status freshness and floating-point SOC handling corrected.

### `C6-001-REV11-AWS`

- N16 RAM-only journey-energy accumulator;
- separate drawn and regenerated Wh counters with stable journey identity;
- checkpoints at journey start, every 60 seconds and at stop/charge boundaries;
- backend can label journey email as `Firmware-Zähler`, with telemetry fallback;
- intentionally disabled on XIAO.

### `C6-001-REV12-AWS` — withdrawn

- first pre-write OTA chip/flash compatibility guard;
- rejected images safely, but used the wrong ESP32-C6 compile-target macro and
  therefore also rejected a matching N16 image;
- requires one-time USB update to REV13 when REV12 is already installed.

### `C6-001-REV13-AWS`

- corrected ESP32-C6 target detection;
- validates Espressif image magic, chip family and declared physical flash size
  before opening the OTA partition;
- physical positive test: matching N16 image accepted;
- physical negative tests: XIAO image rejected for 4/16 MB mismatch and LilyGO
  image rejected for ESP32/ESP32-C6 mismatch;
- failed validation leaves the running firmware unchanged.

### `C6-001-REV15-AWS`

- adds Pioneer-only `0x48D` decoding as the optional
  `bms/soc_internal` and `bms/soc_display` topics;
- keeps `display/soc` as the canonical product SOC and never substitutes either
  Pioneer diagnostic value;
- publishes the independent V2 `0x1B1` values as `bms/standard_soc` and
  `bms/soh_percent` when that profile receives them;
- leaves all optional SOC/SOH values absent when their profile-specific frame is
  unavailable.

### `C6-001-REV17-AWS`

- publishes the configured CAN1 and CAN2 decoder keys as retained AWS birth
  state alongside the firmware version;
- seals and prioritizes the final N16 journey-energy checkpoint immediately when
  fresh Standard-CAN plug or charging state becomes active;
- treats `Speed = 0` only as a stop candidate and seals the counter after ten
  continuous stopped minutes, not on the first zero sample;
- preserves a sealed counter until its final MQTT triplet is accepted before a
  later movement starts a new counter identity;
- leaves the XIAO journey-counter boundary unchanged and disabled.

REV16 was an internal, unpublished journey-fix worktree state. Its revision
counter had been raised while the embedded runtime release string still reported
REV15, so it was deliberately never packaged or activated for WebFlash. REV17 is
the first immutable release containing both that journey fix and the CAN-profile
telemetry.

### `C6-001-REV18-AWS`

- publishes the compile-time hardware identifier as retained AWS birth state at
  `system/board`;
- renames the V1 stable decoder key to
  `standard-can-v1-pioneer-gen1-midrange` and its local UI label to
  `Standard-CAN V1 - Pioneer / Gen1 Mid-Range`;
- preserves numeric decoder profile value `2` and accepts the former
  `standard-can-v1-pioneer` command alias, so stored configuration and service
  workflows remain compatible.

### `C6-001-REV14-AWS`

- publishes `journey/energy_counter_id` as a valid JSON string;
- enables the journey backend to combine the ID with drawn/regenerated Wh values
  instead of falling back to the telemetry estimate;
- keeps the N16 RAM-only journey counter and REV13 OTA compatibility guard
  otherwise unchanged.

## XIAO feature boundary

### Not supported on XIAO

- **Firmware journey-energy counter (REV11+):** the RAM Wh accumulator and its
  `journey/energy_*` checkpoints are N16-only. XIAO journey emails continue to use
  the compatible backend `Telemetrie-Schätzung` path.
- **N16 firmware images:** REV18 deliberately rejects a 16 MB N16 OTA image on a
  4 MB XIAO. XIAO requires the `xiao-esp32c6` image.

### Supported with a reduced limit

- **Offline History cache:** supported, but capped at 128 KiB instead of the
  N16's 256 KiB because XIAO has only 4 MB flash.
- **Firmware growth:** the XIAO image must remain below 85% of one OTA application
slot. REV18 remains subject to the 85% gate, so additional N16 features are not automatically
  enabled on XIAO.

### Still supported in the current XIAO image

- passive Dual-CAN and Display/V1/V2 decoder selection;
- GPS, Home/Mobile WiFi fallback and protected local AP;
- AWS IoT telemetry, ABRP and optional offline History backfill;
- authenticated WebUI, guided onboarding, backup/restore and factory reset;
- local OTA with the REV13+ chip/flash compatibility validation.

XIAO remains a supported compatibility target, but it does not have the same
extended vehicle Dual-CAN/AWS field qualification as the N16 production pilot.
