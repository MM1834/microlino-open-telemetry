# DISPLAY-WEBFLASH-001 — Portal installation for MOT Display

> **Status:** Complete; Display 1.1.0 factory and application-update catalogs active
>
> **Created:** 2026-10-10
>
> **Initial target:** Waveshare `ESP32-C6-LCD-1.28-B`, SKU 35451, 16 MB

## Objective

Let an authenticated pilot who buys the supported display board install the
approved MOT Display firmware over USB from Chrome or Edge without PlatformIO,
a local build environment or a manually selected binary.

Reuse the controlled portal WebFlash infrastructure where its authorization,
immutable-artifact, integrity and audit boundaries fit, but expose a separate
**MOT Display einrichten** flow. Do not present a display installation as an
adapter update.

## Release 1 promotion — 2026-10-13

The physically accepted 1.28-inch firmware is promoted to
`MOT-DISPLAY-1.0.0`. The standard page includes speed, left/right/hazard state,
power, SOC and range; the corresponding adapter contract is canonical in
`C6-001-REV40-AWS`. Indicator state remains local to the adapter/display BLE
link and is not serialized to AWS or local MQTT.

Both existing WebFlash safety modes package successfully from the same reviewed
build. The marker-gated application artifact is 855,808 bytes with SHA-256
`3e8040bda5dc5116d2fb403a9900773b2c04a71ede44c5eb183045510ea2a80e`.
The marker-bearing factory artifact is 921,344 bytes with SHA-256
`aabf9d54454cf3ab8136bc7f6ba213d64b927a563ce6b83dc27f33c78cef679e`.
Both immutable artifacts were uploaded with AES256 server-side encryption and
downloaded again with byte-identical hashes. No-replacement Change Set
`display-webflash-1-0-0-20261013` activated both catalog targets atomically. The
stack reached `UPDATE_COMPLETE`, Lambda is active/successful and recent error
logs are empty. The N16 WebFlash release remains exactly REV38 and existing
display grants were not migrated. Physical browser acceptance of both 1.0.0
paths remains open.

## Display 1.1.0 safety-status update — 2026-10-13

Both display paths now serve `MOT-DISPLAY-1.1.0`. The marker-gated application
artifact is 856,224 bytes with SHA-256
`1e392f648ab8897fb9a577ecac2fa6a0e9a6ae310719a14648607866791dacab`.
The marker-bearing factory artifact is 921,760 bytes with SHA-256
`df7a581a2ad7cad86b254da64f3111ee28be52838464803c2354648f360b0705`.
Both were uploaded AES256-encrypted to the private versioned firmware bucket and
read back byte-identical. Replacement-free Change Set
`display-webflash-1-1-0-20261013` reached `UPDATE_COMPLETE`; Lambda is active
with successful update status and clean logs. XIAO remains REV36 and the normal
N16 WebFlash catalog remains REV38. Existing grants were not migrated.

Live close-out verification on 2026-10-13 confirmed `UPDATE_COMPLETE`, both
1.1.0 catalog parameter sets, the immutable factory/update object sizes and
AES256 encryption in the private versioned firmware bucket. The normal N16
WebFlash catalog remains unchanged.

## Repository implementation 2026-10-10

The first-install path is implemented without changing or reflashing the current
BLE test devices:

- the controlled backend catalog accepts a third, independent
  `waveshare-esp32-c6-lcd-128-b` release and grant;
- explicit target selection lets one pilot hold adapter and display grants
  concurrently without an ambiguous download;
- release metadata distinguishes the display `factory-install` at offset `0x0`
  from the unchanged adapter `application-update` at `0x10000`;
- the portal switches to a separate **MOT Display einrichten** presentation,
  exact model/SKU confirmation and factory-demo replacement warning;
- the browser verifies the exact target, install mode, model, ESP32-C6, 16 MB
  flash, size and SHA-256 before writing the fixed full image;
- `tools/package_display_webflash_release.py` accepts only the PlatformIO
  `firmware.factory.bin` whose bootloader, partition table and application match
  the separately built inputs and whose application contains the expected
  display version marker;
- focused backend, portal and packaging tests pass, and a temporary package of
  the current prototype factory image reproduced its expected hash.

The AWS slice was deployed on 2026-10-10 through no-replacement Change Set
`display-webflash-001-20261010`. The private encrypted/versioned factory image is
`MOT-DISPLAY-0.2-PRE1`, 910,192 bytes, SHA-256
`c71ffc40eebf3cb836fd704b896bf42a1e59d4321a535104c14a237330f9519d`.
Only the existing onboarding Lambda and its inline S3 read policy changed in
place. XIAO `C6-001-REV36-AWS`, N16 `C6-001-REV38-AWS` and all existing grants
remain unchanged. Three target-specific mutation-free probes return cleanly,
anonymous access remains 401 and recent Lambda error logs are empty.

The maintainer uploaded the shared-hosting portal on 2026-10-10. Public read-back
of the dashboard page, administration page, application, WebFlash module,
provider, administration logic and translations is byte-identical to the
reviewed source. The controlled first installation on the second factory-state
display then passed: the vendor demo was replaced, MOT Display started and BLE
connected cleanly to the existing MOT test adapter.

A second portal attempt still offers `factory-install`. This is expected for
`MOT-DISPLAY-0.2-PRE1`: the active catalog contains only the reviewed merged
image at `0x0`, and neither browser-side MOT layout-marker detection nor a
separate application-only display release is deployed. Implementing and proving
that positive marker/partition/target gate at `0x10000` remains open; the portal
must continue failing safe to the full reviewed installation until then.

Repository candidate `MOT-DISPLAY-0.2-PRE2` starts that transition. Its full
image carries a fixed 128-byte layout marker at `0x8C00`, in the unused tail of
the partition-table sector. The marker binds schema v1, the exact Waveshare
target and the SHA-256 of the reviewed partition table, then self-authenticates
its first 96 bytes with a second SHA-256. The packager rejects a nonblank marker
region and produces the marker deterministically. PRE2 also reduces only long
BLE connection footers to text size 1 so `SUCHE GEKOPPELTES MOT` fits inside the
round screen; normal driving-state labels retain size 2. The complete PRE2
factory artifact is installed over USB. Device-side flash verification passed,
its marker read-back matches the manifest hash exactly, and BLE reconnected with
fresh packets after reset. The maintainer accepted the corrected footer on the
physical display before the isolated PRE2 catalog update.

No-replacement Change Set `display-webflash-pre2-20261010` subsequently activated
PRE2 using only the four display artifact/version/hash/size parameters and the
resulting exact display S3 policy ARN. Lambda remains active/successful, XIAO
REV36 and N16 REV38 are unchanged, and recent error logs are empty. The prior
PRE1 display grant remains stored but no longer matches the active release, so it
fails closed; no automatic grant migration occurred.

The renewed PRE2 grant and hosted factory-install flow were then exercised on
both physical Waveshare displays. Both completed the full write, restarted into
MOT Display, retained the corrected footer presentation and connected to the MOT
test adapter. The maintainer accepted both devices. This closes PRE2 factory
installation acceptance; it does not yet satisfy the separate application-only
successor-update criterion.

PRE3 implements that successor as a fourth, separately granted catalog target,
`waveshare-esp32-c6-lcd-128-b-update`. PRE2 remains the factory path. The browser
reads the marker at `0x8C00` and partition table at `0x8000`, then verifies the
marker SHA-256, schema, base target, self-hash, embedded partition digest and
live partition digest before the first write call. A corrupt-marker executable
test confirms rejection after the marker read. The 844,720-byte artifact starts
at `0x10000` and has SHA-256
`f5676af55e5980b1cb5483ac7fe1c3770fe2445f3bffabff9c7bbf2a8135cdeb`.
On the connected display the live preflight passed, PRE3 read back
byte-identical, and the marker remained unchanged. The AWS catalog was then
activated without resource replacement. A transient parameter-only deployment
reset the three optional earlier catalog slots to defaults; the immediately
following atomic four-target correction restored their exact captured values and
retained PRE3. Final Lambda state is active/successful, XIAO REV36, N16 REV38,
factory PRE2 and update PRE3 hashes are all exact, recent error logs are empty,
and no grant was created. Shared-hosting upload/read-back remains pending.

## Scheduling boundary

Deployment and physical acceptance of this sprint are deliberately downstream
of the current MOT Display BLE prototype acceptance. The repository work does
not change the display image, N16 BLE test image, protocol, pairing behaviour or
test procedure planned for the next physical vehicle test.
Evidence from that test may refine later product requirements, but failure or
delay in this sprint is not a gate for the test.

## Accepted product direction

- add a dedicated authenticated portal card or page for MOT Display;
- keep the existing adapter WebFlash targets and their application-only safety
  contract unchanged;
- support one explicitly named board revision first, with a product image and
  exact model/SKU confirmation before any write;
- use the existing server-side expiring grant, private immutable artifact,
  release metadata, SHA-256 verification and bounded result audit model;
- let the browser access the locally attached USB device directly; AWS never
  receives access to the USB device;
- provide a complete first-install image for a factory board and a separately
  constrained application-only update path after MOT ownership of the partition
  layout is proven;
- never expose arbitrary files, offsets, erase controls or a generic ESP32
  flashing surface to a pilot;
- document vendor-demo recovery with an official board-specific image where the
  vendor provides one. The maintainer's local full-flash backup remains private
  and bound to its original physical board.

## Safety contract

The current adapter WebFlash writes only an application at `0x10000` and relies
on an already qualified MOT partition layout. That assumption is not valid for a
new display carrying the Waveshare demo. First installation must therefore write
an approved, release-bound set containing the bootloader, partition table, boot
metadata and MOT Display application, either as explicitly fixed segments or as
one reviewed merged image. The manifest owns every allowed address and length;
the user cannot edit either.

Before the first write, the portal must verify at least:

- ESP32-C6 chip family;
- 16 MB physical flash geometry;
- a supported browser and USB transport;
- exact release target, byte lengths and SHA-256 values;
- explicit user confirmation that the connected unit is the named Waveshare
  `ESP32-C6-LCD-1.28-B` / SKU 35451 and that its factory demo will be replaced.

Espressif native USB identity alone is not sufficient proof of the board model,
because unrelated ESP32-C6 boards may expose the same generic identity. The
implementation must investigate whether a stable, non-secret factory flash
fingerprint can strengthen first-install identification. It must fail closed if
the observed hardware or flash geometry conflicts with the target, but must not
claim unique board identification where none is technically available.

The first MOT installation writes a versioned, non-secret MOT Display layout
marker. Later portal updates may use the fixed application offset only after the
marker, partition table and target identity agree with the release manifest. A
missing or conflicting marker returns the user to the reviewed first-install or
recovery path; it never silently performs an application-only write.

## Release-1 compatibility contract

Publication of MOT Display firmware release 1 establishes a supported baseline,
not a prototype packet that later MOT adapter releases may silently replace.
Every subsequent adapter feature, value, BLE protocol revision or proposed
display screen must be reviewed against the released display firmware before it
is enabled for that display generation.

The compatibility rule is fail-safe and display-owned:

- an adapter update must continue to provide the release-1 data contract while
  a connected display reports only release-1 capabilities;
- new values and screens are additive and must not repurpose existing fields,
  flags, units, scaling, sign conventions or state meanings;
- the display activates a new screen only when its own firmware implements that
  screen and the negotiated/observed data contract is supported;
- release-1 display firmware ignores unknown optional values, flags,
  characteristics and screen identifiers instead of rendering guessed content;
- an unsupported incompatible protocol version or malformed payload is rejected
  as unavailable data without crashing, corrupting settings or replacing the
  last explicit connection/error state with plausible-looking values;
- MOT must not remotely force an older display to open a screen it does not
  know. Screen selection and rendering remain controlled by display firmware;
- if an extension cannot preserve the release-1 baseline, it requires a display
  firmware update first and remains disabled for older display releases.

The pre-release candidate now defines and validates a separate fixed-size
capability-v1 characteristic containing the supported protocol range, screen
bits and feature bits. It retains the stable telemetry-v1 and status-v1
characteristics and layouts; unknown additive capability bits are ignored.
Compatibility tests must still cover at least release-1 display firmware
connected to the newest adapter candidate, as well as current display firmware
connected to the oldest still-supported adapter release.

## User flow

1. The administrator grants the authenticated pilot the approved display
   release independently of adapter-firmware grants.
2. The pilot opens **Settings → MOT Display einrichten** in Chrome or Edge.
3. The portal shows the supported board image/model, required USB cable and the
   warning that the Waveshare demo will be replaced.
4. A user gesture opens the Web Serial chooser.
5. The portal verifies USB compatibility without entering the ROM loader before
   final confirmation, matching the accepted adapter WebFlash behavior.
6. The portal downloads the exact authorized image set and verifies every hash.
7. The pilot confirms the destructive first installation explicitly.
8. Only then does the browser enter the ROM loader, complete chip/flash
   preflight, write and verify the fixed image set, and reset the board.
9. The display starts in MOT mode and reports that it is searching for a
   compatible adapter. BLE pairing/binding remains a separate product step.

## Delivery slices

### A — Display release artifact contract

- deterministic production build for the supported display target;
- reviewed partition layout and fixed first-install segments or merged image;
- manifest with release, target, offsets, lengths, hashes and flash geometry;
- MOT Display layout marker and application-only successor-update contract;
- size and embedded-version gates plus reproducible packaging tests.

### A2 — Release-1 compatibility baseline

- freeze and document release-1 field semantics, units and state behavior;
- add a bounded protocol/capability declaration for optional future screens;
- keep new data and screen support additive and display-controlled;
- add cross-version fixtures proving that release-1 display firmware ignores or
  rejects unsupported extensions while retaining its baseline screens;
- require every later MOT display-facing feature to run the compatibility matrix
  before release.

### B — Authorization and catalog extension

- independent display target and expiring pilot grant;
- private immutable artifact storage and short-lived authenticated download;
- bounded start/success/failure audit without USB content or BLE data;
- no implicit migration from an adapter grant to a display grant.

### C — Portal installation flow

- separate display card/page and localized pilot instructions;
- exact board/SKU presentation and explicit factory-demo replacement consent;
- selection-only first phase followed by confirmed ROM-loader/write phase;
- full-install progress, verification, reset and actionable recovery text;
- later update mode enabled only by an accepted MOT layout marker.

### D — Physical acceptance

Use the second, still factory-state display for the first end-to-end acceptance:

- confirm the original demo runs before installation;
- complete portal grant, download, chip/flash preflight and full write;
- confirm automatic reset and the MOT no-data/search screen;
- confirm BLE connection to the existing test adapter without changing the BLE
  protocol or adapter test image;
- power-cycle and reconnect the display;
- install a later application-only build and prove that the partition table and
  retained display settings remain intact;
- exercise wrong-board, wrong-flash-size, stale-grant, corrupted-hash, cancel,
  disconnect and interrupted-write recovery paths.

## Acceptance criteria

- A pilot can install MOT Display on an untouched supported board using only the
  authenticated portal, Chrome/Edge and USB-C.
- The display target cannot receive an adapter image and adapter targets cannot
  receive a display image through the controlled UI.
- First installation never assumes the vendor partition table matches MOT.
- Later application-only updates require positive MOT layout identification.
- Every authorized artifact is exact-release/hash-bound and every operation has
  a bounded audit outcome.
- No WiFi credential, certificate, BLE payload or USB serial content is sent to
  AWS by the flashing flow.
- The established adapter WebFlash and the current BLE test remain unchanged and
  pass their existing regression/physical checks.
- The newest adapter candidate remains usable with release-1 display firmware;
  unsupported future values or screens are ignored/rejected on the display and
  never appear as misinterpreted data.

## Explicit exclusions

- support for other round displays, generic ESP32-C6 boards or touch variants;
- browser-based arbitrary firmware upload, flash erase or address editing;
- OTA to the display over BLE or WiFi;
- changes to the current BLE packet protocol, pairing policy or display screens;
- public unauthenticated firmware distribution.
