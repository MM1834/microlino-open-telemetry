# WIFI-001 — Preferred Home WiFi and Mobile-WiFi Fallback

> **Status:** Complete; C6 repository and hardware qualification passed
>
> **Started:** 2026-08-07
>
> **Completed:** 2026-08-08

## Objective

Introduce two ordered WiFi station profiles on the ESP32-C6 firmware: the
configured home WiFi is preferred, a second/mobile WiFi such as a dedicated
LTE/GSM hotspot is the automatic fallback, and the firmware automatically returns
to home WiFi when it becomes available again.

The 16 MB nanoESP32-C6-N16 is the primary implementation and qualification target.
The 4 MB XIAO may receive the same implementation only while it remains a shared,
low-cost compatibility build without board-specific architecture or loss of its
validated local-administration boundary. WROOM and LilyGO are sustain-only and do
not receive this feature.

The change must preserve existing AWS IoT identity and publication, local WebUI
access, local OTA, dual-CAN/GPS processing and stored configuration compatibility.

## Authoritative baseline review

Governance, `WORK_ORDER`, `ENGINEERING_BACKLOG`, the firmware documentation index
and the current network/configuration sources were reviewed before defining this
work package. No existing backlog item already owns dual-WiFi failover.

The original platform review found:

- ESP32-WROOM and LilyGO remain useful validated predecessor paths, but their 4 MB
  AWS application partitions are already 99.8% and 99.6% occupied. Further feature
  development on these modules has stopped; existing behaviour remains supported
  as-is while migration is evaluated.
- ESP32-C6 originally stored one WiFi profile through the serial console and had
  neither setup AP nor local WebUI. C6-PH-001 subsequently closed that prerequisite
  with a protected setup/fallback AP, authenticated WebUI, backup/restore, factory
  reset, shared local OTA and cooperative reconnect. N16 hardware acceptance and
  XIAO compatibility validation passed on 2026-08-07.

The C6-PH-001 repository revision was re-reviewed at WIFI-001 continuation. Its 25
focused C6/WROOM/LilyGO security tests and all eight maintained base/AWS builds
passed again. The shared Web-security, OTA and physical administrator-recovery
changes do not alter AWS identities, telemetry topics or LilyGO LTE selection.

The XIAO 4 MB AWS build uses 1,283,020 of 1,703,936 application bytes (75.3%) after
the initial C6 dual-profile configuration contract. Its remaining margin is
adequate for evaluation, but N16 remains authoritative and no XIAO-only fork is
permitted.

## Required behaviour

The ordered station profiles are:

1. `home`: preferred whenever its configured SSID is visible and association
   succeeds;
2. `mobile`: used only while home WiFi is unavailable or cannot be joined.

The network manager must:

- validate and store both profiles without logging or rendering passwords;
- preserve existing single-profile installations by migrating the existing
  `wifiSsid`/`wifiPass` values to the home profile;
- scan periodically without continuously interrupting the active connection;
- connect to home first at boot when both networks are available;
- fall back to mobile WiFi after a bounded home failure/loss grace period;
- while on mobile WiFi, periodically scan for home and return only after a
  successful home association;
- use bounded, non-blocking retries/backoff so CAN, GPS, WebUI, MQTT/AWS and modem
  loops continue running;
- avoid oscillation through a stable-connection interval and retry backoff;
- expose the selected profile, scan/retry state and transition reason in
  diagnostics without exposing credentials;
- notify transport clients through normal link loss/reconnect behaviour rather
  than changing MQTT topics, AWS Thing/client identity or credentials.

## Platform fallback policy

### ESP32-C6

If neither configured WiFi can be joined, start the device AP. Keep AP+STA mode
available while scanning and attempting recovery. Once either station connection
has remained stable for the defined interval, stop the fallback AP. If that
connection is later lost and neither profile can be joined, restart the AP.

The C6-PH-001 protected AP and local-administration boundary is now the accepted
starting point. WIFI-001 must preserve its setup credential, authenticated WebUI,
backup/restore, factory reset and OTA behaviour.

WROOM and LilyGO receive no runtime or configuration changes in this sprint. The
external mobile hotspot replaces an onboard cellular dependency for the new C6
direction; selection of a future LilyGO hardware replacement remains a separate
architecture decision.

## AP and WebUI request handling decision

Network switching will not be gated on an "active WebUI session". The current
local WebUI uses HTTP Basic authentication and short-lived requests; it has no
authoritative application session whose lifetime the network manager can safely
observe. A stale browser connection must also not indefinitely block recovery to
the preferred network.

Instead:

- AP+STA remains active throughout scanning and association;
- an in-flight configuration response must be completed before any intentional
  restart or immediate AP shutdown;
- the AP is stopped only after station connectivity passes the stable interval;
- failed association leaves or restores the AP without waiting for browser state.

This gives a deterministic network state machine while minimizing interruption
of local administration.

## Configuration and UI contract

- Keep `wifiSsid` and `wifiPass` as the home-profile JSON keys for backward
  compatibility.
- Add explicit second-profile keys (provisionally `wifi2Ssid` and `wifi2Pass`) in
  the shared configuration contract and all applicable backup/import paths.
- C6 WebUI and serial commands must label/support preferred home WiFi and the
  second/mobile hotspot, preserve blank stored passwords, inspect without secret echoing and
  clearing each profile independently.
- Configuration export excludes both passwords unless secrets were explicitly
  requested under the existing protected backup contract.

## Delivery slices

### WIFI-001.A — Shared contract and deterministic policy tests

- add second-profile configuration keys and backward-compatible persistence;
- define platform-neutral selection inputs/outputs or equivalent focused policy
  tests covering priority, fallback, return, backoff and anti-flap behaviour;
- add source/contract tests for secret handling and platform fallback policy.

**Completed:** The shared `wifi2Ssid`/`wifi2Pass` backup keys plus C6 NVS
persistence, import/export redaction and set/clear paths are implemented. The
existing profile remains the home profile without an NVS rewrite. Four focused
dual-profile contract tests pass together with the C6/local-security tests. The
temporary WROOM/LilyGO dual-profile additions were removed after the product-scope
decision.

### WIFI-001.B — ESP32-C6 runtime and administration

- add both profiles and preferred-network recovery to the hardened C6 line;
- expose both profiles and secret-safe diagnostics in WebUI and serial recovery;
- preserve the protected fallback AP/recovery/OTA boundary from C6-PH-001;
- compile N16 base/AWS first, then compile XIAO from the same source without a
  compatibility fork;
- recheck simultaneous dual-CAN/GPS/AWS operation on N16.

**Repository result:** Complete. The cooperative state machine attempts home
first, moves to the mobile profile after a bounded 15-second home timeout, starts
the protected fallback AP only after all configured profiles fail, retries after
30 seconds and stops the AP only after 10 seconds of stable station connectivity.
While mobile WiFi is active it runs an asynchronous home scan every 60 seconds and
returns when the preferred SSID is visible. There are no blocking WiFi loops or
delays.

The authenticated setup/configuration WebUI, non-secret backup/import contract,
serial `wifi`/`wifi2` commands and JSON diagnostics cover both profiles without
echoing passwords. Diagnostics expose `state`, active `profile` and transition
`reason`.

All 107 repository tests and all four C6 base/AWS builds pass. Relative to the
configuration-only baseline, the complete runtime/UI/diagnostic slice adds 5,222
bytes to every C6 image and approximately 48–56 bytes RAM:

| Environment | Before | After | Application usage |
|---|---:|---:|---:|
| `nanoesp32c6-n16` | 1,143,772 B | 1,148,994 B | 21.9% |
| `nanoesp32c6-n16-aws` | 1,294,812 B | 1,300,034 B | 24.8% |
| `xiao-esp32c6` | 1,131,976 B | 1,137,198 B | 66.7% |
| `xiao-esp32c6-aws` | 1,283,020 B | 1,288,242 B | 75.6% |

This is repository evidence only. Physical transition, AP continuity, AWS
reconnection and concurrent dual-CAN/GPS acceptance remain in WIFI-001.C.

### WIFI-001.C — Hardware qualification

Run repeatable tests with controlled SSID availability:

- both present at boot selects home;
- loss of home selects mobile WiFi;
- restored home causes a bounded return;
- loss of both starts the protected C6 fallback AP;
- wrong password, invisible SSID and rapid availability changes do not create a
  tight retry loop or prevent WebUI/CAN/GPS processing;
- AWS/legacy MQTT reconnect with the same identity and no duplicate publisher;
- local WebUI and OTA remain usable on their documented network boundaries.

**XIAO compatibility evidence, 2026-08-08:** The shared XIAO-AWS image flashed
without erasing NVS. Both profiles and local-administrator configuration survived.
With the phone hotspot temporarily assigned as preferred WiFi 1 and the house
network as WiFi 2, the device:

- timed out the unavailable preferred hotspot after 15 seconds and connected to
  WiFi 2 as `profile=mobile`;
- asynchronously detected the 2.4 GHz hotspot after iPhone "Maximize
  Compatibility" was enabled and returned to it as `profile=home` with address
  `172.20.10.2`;
- exposed a physical ESP32-C6 driver edge case after hotspot loss: `WL_CONNECTED`
  remained set while the address had already become `0.0.0.0`;
- passed the repeated fallback after the online predicate was hardened to require
  both `WL_CONNECTED` and a non-zero local address;
- returned to the final configuration with house WiFi as preferred Home and the
  disabled phone hotspot retained as WiFi 2;
- retained a valid GPS fix and continued both CAN-controller loops throughout all
  observed transitions.

The XIAO had no usable AWS credentials during the transition test, so AWS reconnect
was not claimed. With explicit operator authorization on 2026-08-08, its complete
flash was subsequently erased and the current 4 MB partition layout, XIAO-AWS
image and an empty valid LittleFS image were installed. The clean boot reported no
configured WiFi, no AWS credentials and the protected fallback AP, while retaining
a valid GPS fix and both CAN-controller loops. This leaves the module ready for a
separate onboarding exercise without treating onboarding as WIFI-001 scope. No CAN
frames were connected during this USB compatibility test. Protected
AP-after-both-loss, AWS reconnect and live dual-CAN transition evidence remain N16
acceptance gates.

**N16 vehicle evidence, 2026-08-08:** The N16-AWS image was installed by OTA and
both profiles were configured. At the initial vehicle location the Home network
was apparently below usable range, so the module selected the mobile hotspot and
continued publishing vehicle data. On entering the garage it detected the covered
Home network, automatically returned to the preferred profile and propagated the
new Home-network address to the portal. This physically confirms mobile fallback,
Home polling, preferred-network return and AWS/portal continuity on N16. A
subsequent USB-observed road run captured the complete transition: Home degraded
to about -93 dBm and disconnected; after the bounded Home attempt the N16 selected
Mobile at `192.168.100.114`; AWS reconnected and publishing resumed. On the return
route the periodic scan found Home and the N16 changed back to `192.168.11.28`,
with AWS connected again. CAN1 and CAN2 counters increased throughout with zero
reported controller errors, BMS values remained live and GPS acquired and retained
a valid fix. Only the controlled loss of both networks with protected-AP recovery
remained as the principal N16 runtime gate.

The final controlled test removed the only reachable network by disabling the
mobile hotspot outside Home coverage. The N16 exhausted the bounded Mobile attempt
and entered `fallback-ap` with its protected `MOT-*` AP. AWS correctly waited for
network while the GPS fix, live BMS values and both error-free CAN counters
continued. After the hotspot was restored, the AP retry policy checked Home first,
connected to Mobile at `192.168.100.114`, re-established AWS publication and then
stopped the fallback AP after the station connection remained stable. This closes
the N16 runtime gate.

## Completion evidence

Repository completion requires focused tests plus successful N16 base/AWS builds.
XIAO base/AWS builds are a compatibility gate for including that board, not a
reason to constrain N16 architecture. Runtime and repository completion evidence
is recorded above. XIAO remains supported for this feature
only if it passes the same source/build path and a bounded WiFi/AP acceptance
test. WROOM and LilyGO are
outside WIFI-001 completion evidence.

## Post-completion XIAO antenna correction

On 2026-08-09 the maintained XIAO profiles were found to select the external U.FL
path even though deployed XIAO units use their onboard ceramic antenna. The
standard base/AWS profiles now explicitly select the internal antenna and report
the selection at boot. External U.FL remains an opt-in build flag only for a unit
with a connected 2.4 GHz antenna. N16 profiles and WIFI-001 network policy are
unaffected.
