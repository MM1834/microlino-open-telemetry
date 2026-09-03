# WEBFLASH-001 — Controlled Portal Web Flasher

> **Status:** Active
>
> **Started:** 2026-09-01
>
> **Targets:** nanoESP32-C6-N16 and Seeed XIAO ESP32-C6

## Objective

Let an authenticated pilot install an administrator-approved MOT firmware image
over USB from Chrome or Edge before local device onboarding, without PlatformIO,
a manually downloaded binary or access to a factory erase operation.

The portal must treat visibility as convenience only. The backend authorizes the
exact Cognito subject, target, image and expiry before firmware bytes become
available. The browser then flashes the locally attached adapter through Web
Serial; AWS never receives access to the user's USB device.

## Accepted product direction

- place the flasher under authenticated portal settings;
- require an explicit, server-side administrator grant for the pilot phase;
- allow a grant to name one release target and expire automatically;
- host the approved image so the user never selects a local file;
- use a configuration-preserving N16 application update at offset `0x10000`;
- never expose factory erase, arbitrary offsets or arbitrary binaries to pilots;
- verify chip family, flash geometry, release identity and SHA-256 before write;
- record grant, start, success and failure as bounded audit events;
- keep NVS, LittleFS, AWS certificates and device identity outside the write set.

## Security contract

The first release target is `nanoesp32c6-n16`. Its approved artifact is the
PlatformIO application `firmware.bin`, not `firmware.factory.bin`. A pilot write
is restricted to offset `0x10000`; the UI cannot edit the address or enable erase.
An ESP32-C6 and 16 MB flash read-back are mandatory before the confirmation step.
Any mismatch fails closed before the first flash write.

Firmware metadata is immutable per release and contains at least target, semantic
version, byte length and SHA-256. The binary is stored outside the public portal
tree. An authenticated API returns a short-lived download authorization only when
the caller has an active matching grant. Browser-side hiding, a guessed URL or a
stale cached manifest never grants access.

The image contains no per-device AWS credentials. Existing credentials remain in
LittleFS because the permitted write range does not include that partition. This
sprint does not claim signed-image rollback or secure boot.

## User flow

1. Administrator grants one user access to the approved N16 release for a bounded
   period.
2. The authenticated user sees **Settings → Adapter firmware**.
3. The page explains Chrome/Edge, USB-C, vehicle disconnection and configuration
   preservation.
4. A user gesture opens the Web Serial device chooser.
5. The flasher identifies ESP32-C6 and 16 MB flash, then displays release and
   checksum information.
6. The user confirms **Update firmware – keep configuration**.
7. The browser fetches the approved image, verifies its SHA-256, writes only the
   application range and verifies the result.
8. The adapter restarts; the portal reports the outcome and points the user to the
   local onboarding wizard.

## Delivery slices

### A — Release artifact contract

- deterministic N16 application artifact and metadata generation;
- checksum and size validation;
- explicit rejection of factory images and incompatible targets.

### B — Authorization backend

- **Implemented and deployed 2026-09-01:** encrypted TTL grant table keyed by
  Cognito subject and firmware target;
- admin grant/revoke operations restricted to `mot-beta-admins`;
- authenticated self-status and five-minute artifact authorization;
- conditional transactional writes and bounded audit retention;
- private AES256/versioned S3 artifact storage with every public-access block set.

The deployed immutable first release is `C6-001-REV14-AWS`, 1,438,064 bytes, with
SHA-256 `9eeb9f20faf571ba65e03cec5c653b688ec0c35010990c484b6679d72cb95488`.
Both reviewed Change Sets modified existing resources in place; the stack returned
to `UPDATE_COMPLETE`. No user has yet received a firmware grant.

The successor `C6-001-REV15-AWS` was uploaded and activated on 2026-09-03. A
release-integrity review then found that the first REV15 manifests had been
derived from `MOT_REVISION=REV15` while the binaries still embedded the older
`C6-001-REV14-AWS` display/runtime string. Those mislabeled objects are no longer
active. The corrected N16 application is 1,438,992 bytes with SHA-256
`19d43d83626bfeceeb203ffdd241d6c5fd10f2c16ba5e731faf14f02bdaaad86`;
the corrected XIAO application is 1,425,552 bytes with SHA-256
`4314da2f03266a0a8ab406ab73cfe53b78c67e5a7b364e510ba27b1015a214a1`.
Both manifests retain the fixed `0x10000` application-only write and preserve
NVS, OTA metadata, LittleFS and AWS credentials. S3 read-back confirms the exact
sizes, AES256 encryption and versioned object IDs. The stack and Lambda
configuration read back `C6-001-REV15-AWS` and the corrected hashes for both
targets after replacement-free Change Sets
`webflash-rev15-corrected-xiao-20260903` and
`webflash-rev15-corrected-n16-20260903`. Existing grants bound to older hashes
fail closed. The packaging tool now also requires the declared release string to
occur in the application binary, preventing a recurrence. `xruser` received a
new audited 48-hour XIAO grant for `xrpioneer2`; exact-principal access read-back
returns only the corrected 4 MB XIAO artifact.

### C — Portal flasher

- **Implemented in the repository 2026-09-01:** authenticated settings card in
  German, English and French, visible only after a successful backend access check;
- administrator grant/revoke form on the dedicated role-protected Administration
  page delivered by ADM-UX-001;
- vendored Apache-2.0 `esptool-js` 0.6.0 integration over Web Serial;
- exact CH343 USB, ESP32-C6 and 16 MB flash preflight, followed by byte-length and
  SHA-256 verification before the first write;
- fixed application-only write at `0x10000`, `eraseAll: false`, deterministic
  progress, result audit and unsupported-browser states;
- local desktop and 390 px portal smoke tests pass without horizontal overflow.
- smartphone layout assigns the authorized flasher an explicit order immediately
  after personal settings; desktop document order is unchanged.

### D — Acceptance and rollout

- local negative tests for unauthorized, expired and wrong-target access;
- physical configuration-preserving update on B025;
- macOS Chrome/Edge acceptance, followed by Windows Chrome/Edge acceptance;
- hosted authorization regression and one controlled remote-pilot trial.

The production upload, physical B025 configuration-preservation run and native
Windows/macOS Chrome/Edge acceptance remain open.

### First hardware finding — B025

The first portal preflight reached the ESP32-C6 ROM and uploaded the flasher stub,
but communication failed immediately after `esptool-js` changed the CH343 serial
link from 115200 to 460800 baud. Its fallback then reported 4 MB even though no
valid flash ID had been read. Keeping the stub at 115200 did not fix its SPI flash
access on B025, although chip detection and stub upload remained successful. The
portal therefore now stays in the ESP32-C6 ROM loader at 115200, explicitly
rejects flash IDs `0x000000`, `0xffffff` and unknown capacity codes before size
detection, and translates busy-port/synchronization failures into actionable
instructions to close PlatformIO or other serial monitors and reconnect USB.
The next B025 run confirmed that ROM attach completed but RDID still returned
zero. Comparison with current official Python `esptool` identified two stale C6
values in `esptool-js` 0.6.0: it uses the C3 SPI base `0x60002000` instead of the
C6 SPI2 base `0x60003000`, and sends only four bytes for the ROM `SPI_ATTACH`
request instead of eight. The portal applies both C6 corrections locally before
reading the JEDEC ID; physical confirmation remains pending.

The following B025 run passed the complete ESP32-C6/16 MB preflight and checkbox
gate. Its first artifact fetch failed because the Lambda-generated presigned URL
used the global `s3.amazonaws.com` endpoint. S3 returned a CORS-less HTTP 307 to
the regional endpoint, which browsers surfaced only as `Failed to fetch`. The
deployed Lambda now signs directly against `s3.eu-north-1.amazonaws.com`; a live
Lambda-generated URL returns HTTP 200, the exact portal CORS origin and the
expected 1,438,064-byte content length without a redirect. During the hotfix an
initial Change Set reset optional release parameters to defaults; a reviewed
replacement-free corrective Change Set immediately restored the exact release
metadata and `demo-pioneer` read-only guard. The stack is `UPDATE_COMPLETE` and
the existing user grant is active again.

The subsequent B025 write reached the ESP32-C6 ROM but its compressed-flash start
was rejected with status `0x0105`. `esptool-js` 0.6.0 sends only the legacy four
32-bit fields for `FLASH_DEFL_BEGIN` on C6, while current official `esptool` also
sends the required `encrypted_write` field for every post-ESP32 ROM. The portal
now installs a narrowly scoped C6 ROM override that sends the fifth field as zero;
the physical retest confirmed the patched command was sent but the C6 ROM itself
still rejected compressed flashing. The portal therefore performs its fail-closed
chip/JEDEC/16 MB checks in ROM first, then starts the bundled C6 flasher stub at
the unchanged 115200 baud and verifies that the stub sees the identical JEDEC ID
before enabling the write. The compatibility override also emits the fifth field
only for the ROM; the older bundled stub retains its 16-byte request and receives
the stub-specific unrounded byte count. An independent read-only check with
official `esptool` 5.3.0 successfully loaded its stub and detected JEDEC
manufacturer `0x68`, device `0x4018` and 16 MB on B025. Physical portal-write
confirmation then passed: the application was written with visible progress and
technical logging, configuration was preserved and the adapter restarted. The
legacy cloud-vehicle status card directly below the USB flasher was removed after
acceptance because its firmware/device/RSSI/uptime values were misleading in that
location; local WebUI addressing remains available in the existing network views.

The controlled second hardware profile was activated on 2026-09-01 for a XIAO
pilot test. The maintained `xiao-esp32c6` AWS application passed its 85% OTA-slot
gate at 1,424,624 bytes (83.61%) and was packaged as immutable
`C6-001-REV14-AWS`, SHA-256
`986dff0827a228d46809ba2d3fe5253f0425773a662540c10d9c977ca405aea1`.
The portal now accepts only the exact N16/CH343 or XIAO native-USB identities and
checks 16 MB or 4 MB respectively. Replacement-free Change Set
`webflash-xiao-20260901` switched the single active backend release to XIAO while
preserving all tables, bucket, claim controls and `demo-pioneer` guard. The N16
grant for `info@muehlberg.ch` was atomically revoked and replaced by a 48-hour
audited XIAO grant; deployed access read-back returns the exact 4 MB release.
Because no local XIAO remained available for physical acceptance, the same exact
release was also granted for 48 hours to the confirmed pilot account
`gino@microlino-open-telemetry.ch`. This is a controlled first-hardware test:
image build, 4 MB gate, official XIAO USB identities and all pre-write checks are
validated, while native-USB reset/re-enumeration and the physical write/restart
remain explicitly unconfirmed until the pilot reports the result.

Replacement-free Change Set `webflash-multitarget-20260901` then converted the
single-release configuration into a bounded two-target catalog without replacing
tables, bucket, API or grants. Exact target/version/SHA matching remains mandatory,
and more than one matching active grant for the same user fails closed. Live
Lambda read-back simultaneously returned XIAO/4 MB for Gino and N16/16 MB for
`info@muehlberg.ch`; the latter was atomically switched back from XIAO to N16 with
both actions audited.

The maintainer additionally confirmed in the hosted portal that the unsupported-
browser/device compatibility guard behaves as designed. The confirmed but still
first-login-pending account `christian@reding.com` received a separate audited
48-hour N16 grant; deployed Lambda read-back returned the exact 16 MB N16 release.

## Explicit exclusions

- Safari and Firefox Web Serial support;
- WROOM or LilyGO images;
- factory reset or complete-flash installation;
- arbitrary user-supplied files or flash offsets;
- remote OTA, AWS IoT Jobs or flashing without a local USB connection;
- public firmware downloads.

## Initial acceptance gates

- an unauthenticated or unapproved user cannot obtain artifact authorization;
- an admin grant is exact, expiring, revocable and audited;
- wrong chip, wrong flash size, wrong target, size mismatch and checksum mismatch
  all stop before writing;
- successful B025 update preserves NVS, LittleFS and AWS credential state;
- no pilot-facing control can erase the device or write outside the approved
  application range;
- the portal remains usable when Web Serial is unavailable.

## Relationship to FW-CHECK-001

WEBFLASH-001 absorbs the previously parked informational target/version manifest
work from FW-CHECK-001. The stricter hardware-aware compatibility result becomes
a mandatory pre-write gate here rather than a standalone informational feature.
The older proposal remains audit context and is not implemented independently.
