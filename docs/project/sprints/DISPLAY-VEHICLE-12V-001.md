# DISPLAY-VEHICLE-12V-001 — Tyre pressure and 12-V board-network pages

> **Status:** Repository foundation candidate; physical acceptance open
>
> **Reserved:** 2026-10-20
>
> **Scope:** Additive adapter/display contract with an optional later portal consumer

## Goal

Add two local MOT Display data pages without changing any released adapter
packet, existing display page or older display behaviour:

- **Vehicle information:** front-left, front-right, rear-left and rear-right tyre
  pressure plus current 12-V board-network voltage;
- **12-V board network:** current voltage, most recent bounded minimum and
  maximum, and a small bounded history graph.

The paired adapter remains the authoritative acquisition and normalization
source. The local display function must not require AWS, WiFi or the portal.

## Repository increment 2026-10-20

The first vertical slice is implemented:

- `vehicle-board-network-v1` is a separate optional 51-byte encrypted BLE
  characteristic and capability bit; every released record remains unchanged;
- the BM2/BM6 diagnostic adapter exposes decoded live voltage and, for BM2, the
  most recent twelve two-minute history samples plus bounded minimum and maximum;
- the display client discovers, reads, validates and subscribes to the optional
  record while remaining compatible with adapters that omit it;
- the 1.85C prototype uses one combined **Vehicle & 12 V** page for all four
  wheel positions, current/minimum/maximum voltage and the bounded trend;
- tyre-pressure validity remains false because no authoritative decoder or wheel
  assignment exists yet. The screen renders those positions unavailable.

## Local battery-monitor assignment increment 2026-10-21

REV48 replaces the product-level fixed monitor filter with an authenticated
local assignment workflow. The adapter WebUI opens an explicit two-minute BLE
search, presents bounded BM2/BM6 candidates, stores the selected address and
protocol in NVS and supports deliberate unassignment. Outside that window an
unassigned or different nearby monitor is ignored. A selected device still has
to pass the existing FFF0/FFF4, payload and plausibility checks before values
become authoritative.

The persistent registry already contains separate board-battery, front-left,
front-right, rear-left and rear-right slots. The four tyre slots are not yet
assignable because real sensor identity and protocol qualification remain open.
No portal dependency or remote mutation route was added. No existing display
record changed. A later display increment may use a separate encrypted local
management characteristic and explicit service gesture.

Two production builds pass: the universal image has no preselected monitor;
the B.025 transition image writes its previously compiled AEG address into NVS
once and is thereafter locally changeable.

The REV48 B.025 transition application was written at `0x10000` and hash-
verified on 2026-10-21, preserving NVS and the remaining flash layout. Runtime
read-back shows the migrated `9c:1d:58:12:74:6e`/`bm2` assignment, a successful
FFF0/FFF4 connection, `12.94–12.95 V`, one two-minute history sample and a clean
bounded disconnect. The DualEye encrypted link remained connected and advanced
from 31 to 378 notifications during the observation. The maintainer subsequently
accepted the complete local lifecycle on the same hardware: explicit
unassignment, bounded search, selection and assignment of the intended monitor,
continued presentation on DualEye, and persistence across adapter and display
restarts all pass. Further BM2/BM6 models and display-originated administration
remain separate follow-ups rather than gates for this WebUI workflow.

The cloud alarm path is also physically accepted. After the user enabled
notifications for `ml-pilot-025`, a qualified `SUPPLY_UNDERVOLTAGE` event at
`11.36 V` was stored once as `DISPATCHED`. Its delivery evidence contains an
email message identifier and SMS status `SENT`; the maintainer confirmed receipt
of both email and SMS on the intended handset/account. Read-only preflight also
reports every SMS gate true for both `ml-pilot-025` and `pioneer`.

The adapter diagnostic, 1.85C display and 1.28-inch compatibility builds pass.
On 2026-10-20 the 1.28-inch non-touch C6 and 1.28-inch S3 DualEye targets were
flashed successfully with write verification. The DualEye hardware baseline,
including both displays, touch/page input, GPIO0 page input and BLE scan startup,
was accepted physically. B.025 then connected to the isolated AEG Battery
Monitor `9C:1D:58:12:74:6E`, discovered FFF0/FFF4 and subscribed without a
configuration write. The first capture repeatedly decoded `13.42 V` with zero
stored samples. After the final firmware restart, a second capture repeatedly
decoded `12.50 V` and retrieved one two-minute history sample with matching
minimum and maximum. The adapter now repeats the bounded connection, live
capture and history query 60 seconds after each successful release. Two
successive cycles passed physically after fixing reuse of the preceding
history-complete state: the first read `13.00–13.01 V` and six samples over 12
minutes; the second read `12.52 V` and again completed all six samples. The
bounded history range was `12.50–13.37 V`, enabling the trend graph. The
encrypted display link remained connected and continued notifying throughout.
No missing history is synthesized, and live voltage is invalidated after five
minutes without a new monitor sample. Visual acceptance of the updated DualEye
values and trend remains open. Separate 1.28-inch non-touch
vehicle-information and board-network pages are the next increment.

For the UI and end-to-end contract test, B.025 now publishes an explicitly
temporary simulated tyre set: front-left/front-right `1.90 bar`, rear-left
`1.70 bar` and rear-right `1.45 bar`. The values use the existing four kPa
fields and validity bits; no released packet changed size. The threshold profile
is red below `1.50 bar`, amber from `1.50` to below `1.80 bar`, green from
`1.80` to below `2.00 bar`, yellow from `2.00` through `2.50 bar`, and red above
`2.50 bar`. After the accepted green/green/green/amber baseline, rear-right was
lowered from `1.70` to `1.45 bar` for the red display and alarm-path test; the
current set therefore renders green/green/amber/red.

DualEye and the 1.85C build apply the same thresholds. The general portal has a
dedicated responsive **Fahrzeug-Information** panel, while Fahrt and MOT Lite
remain unchanged. AWS/local MQTT carry four pressure topics and a derived red
alarm boolean. Separate email/SMS opt-ins dispatch once on entry into a red
alarm phase; amber and yellow do not notify. The simulated source is labelled in
the portal and must be replaced by a physically qualified sensor decoder before
production acceptance. B.025 and DualEye were rebuilt, flashed and hash-verified
on 2026-10-20; the post-flash adapter log confirmed an active encrypted display
connection. The 1.85C target builds but remains unflashed as requested.

## GEL 12-V threshold profile

The candidate profile is intentionally explicit and temperature-agnostic until
the monitor provides a qualified battery temperature:

- normal: `12.20–14.20 V`;
- amber: `11.80–12.19 V` or `14.21–14.40 V`;
- red: below `11.80 V` or above `14.40 V`;
- cloud alarm qualification: the same red condition in three consecutive
  60-second capture cycles, with recovery hysteresis at `12.00 V` and `14.20 V`.

The limits are based on the manufacturer GEL charging profile (approximately
`14.2 V` absorption and `13.7–13.8 V` float) and conservative low-voltage
protection guidance. Voltage alone is not a state-of-charge measurement, and
brief starting/load transients are not promoted to a notification.

The existing 51-byte BLE record uses previously reserved flag bits for low/high
amber and red states, so its length and all older records remain unchanged.
DualEye and 1.85C render the current voltage and state in green, amber or red.
B.025 and DualEye were rebuilt, flashed and hash-verified. The AWS and local
MQTT envelopes now optionally include voltage plus qualified under/overvoltage.
The general portal dashboard has a responsive dedicated **12-V-Bordnetz** panel,
and the existing opt-in notification channel covers qualified low and high GEL
alarms. Repository builds, JavaScript syntax, 36 focused Notification/template
tests and whitespace validation pass. The static portal was uploaded on
2026-10-20. The hosted general dashboard exposes the dedicated panel and its
GEL limits; a 667-pixel-wide browser check showed the single-column responsive
layout without horizontal overflow. Authenticated live-data acceptance, backend
deployment and controlled physical threshold injection remain open.

The B.025 cloud path now publishes a fresh BM2 sample after adapter start, on
every detected transition between driving, off and charging, and otherwise no
less often than every five minutes while a valid sample exists. The envelope
labels this source as `bm2_ble`; MOT Lite labels its independent GPIO/ADC value
as `adapter_adc`. Both use the same canonical `system/supply_voltage` value but
remain distinguishable in State, History and the portal. AWS stores only the
bounded five-minute live series and its source label; the monitor's detailed
two-minute history remains adapter/display-local and is not bulk-uploaded.

On 2026-10-20 B.025 was rebuilt and flashed with the BM2 feature explicitly
enabled in its target profile. It reconnected to the assigned monitor
`9C:1D:58:12:74:6E`, read `12.14 V`, retrieved 46 two-minute records spanning
92 minutes, and released the BLE connection. State read-back for
`ml-pilot-025` contains `12.14 V`, `bm2_ble`, source age zero and both alarm
flags false. The bounded History table contains the matching voltage/source
bucket. `ml-pilot-025` was appended to the existing History allowlist; no other
assignment or vehicle identity was changed.

## Start gates

- Identify and qualify an authoritative tyre-pressure source, including wheel
  assignment, units, validity, freshness and unavailable states.
- Promote the diagnostic BM2/BM6 path only after its exact protocol family,
  encrypted history format, sample order, freshness and overlapping-read
  deduplication are accepted.
- Qualify BM2/BM6 coexistence with MOT Display BLE, Dual CAN, WiFi/AWS and GNSS.
- Define one bounded canonical 12-V summary suitable for both the display and an
  optional later authenticated portal consumer.

The current official adapter contract contains neither qualified tyre-pressure
values nor production 12-V history. Placeholder-only display pages do not meet
the start gates or count as implementation.

## Compatibility contract

- Introduce one separately versioned, optional encrypted BLE characteristic.
- Do not enlarge, reinterpret or replace released telemetry-v1, status-v1,
  extended-data, driving-status or other established records.
- Older adapters omit the characteristic. Older displays ignore the extension
  and retain their existing pages.
- Every value is individually validity- and freshness-gated; missing data is
  shown as unavailable and is never estimated.
- The adapter sends only bounded display-ready samples and summary values, not
  the monitor's complete raw history.
- The 12-V values always refer to the separate board-network battery; traction-
  pack voltage must never be substituted.

## Data contract to define

The implementation increment must specify and test:

- canonical pressure and voltage units plus integer scaling;
- four stable wheel positions and per-wheel validity/freshness;
- current voltage source and timestamp/age semantics;
- bounded minimum/maximum window semantics and reset behaviour;
- fixed maximum history sample count, cadence, ordering and gap representation;
- protocol version, payload length, integrity checks and unavailable-state
  encoding.

## Portal boundary

After the adapter/display dataset and local opt-in contract are accepted, the
same canonical 12-V summary may be added to the authenticated portal. Portal
retention, privacy, freshness and unavailable-state behaviour require explicit
acceptance. AWS remains optional and is not on the critical path for either MOT
Display page.

## Acceptance outline

- [ ] Tyre-pressure source and all four wheel assignments are physically
  qualified (temporary fixed-value transport/rendering test is active).
- [ ] BM2/BM6 current and bounded history decoding are physically qualified
  (AEG/BM2 live voltage and one-sample history pass; multi-point BM2 history and
  BM6 remain open).
- [x] BM2 live-voltage coexistence with the encrypted display link passes on
  B.025; broader CAN/WiFi/AWS/GNSS coexistence remains part of release testing.
- [x] 1.28-inch non-touch and DualEye firmware flash/write verification pass.
- [x] DualEye display, touch/page input, GPIO0 input and BLE scan baseline pass.
- [ ] New-adapter/old-display and old-adapter/new-display compatibility tests
  pass.
- [ ] Both displays render fresh, stale, partially missing and unavailable data
  deterministically.
- [ ] The history graph remains within the agreed payload, memory and rendering
  bounds.
- [ ] Local operation passes with AWS and WiFi unavailable.
- [x] REV48 local scan/select/unassign and reboot persistence pass physically on
  B.025; compiler qualification passes for universal and B.025 migration builds.
- [ ] Relevant firmware builds and focused protocol/rendering tests pass.
- [x] The hosted general portal exposes the responsive 12-V panel outside Fahrt
  and MOT Lite; authenticated live-data acceptance remains open.

## Related documents

- [MOT Display](../../hardware/mot-display.md)
- [Vehicle board-network power and 12-V battery monitoring](../../hardware/vehicle-board-power.md)
- [Engineering backlog](../../governance/ENGINEERING_BACKLOG.md#mot-display-vehicle-information-and-12-v-board-network-pages)
