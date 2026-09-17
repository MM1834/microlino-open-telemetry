# WEBFLASH-001 — Controlled Portal Web Flasher

> **Status:** Active
>
> **Started:** 2026-09-01
>
> **Targets:** nanoESP32-C6-N16 and Seeed XIAO ESP32-C6

> **Safety hold lifted 2026-09-17:** Two physical runs accepted the explicit
> reset pulse. The previously valid N16 users now have exact REV29 grants.

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

`C6-001-REV16-AWS` remained an internal, unpublished revision: the journey fix
had raised `MOT_REVISION`, but the application still embedded the REV15 runtime
version and therefore could not pass the WebFlash packaging contract. It was
never activated. The consolidated successor `C6-001-REV17-AWS` was built,
packaged and activated on 2026-09-07 for both targets. The N16 application is
1,439,600 bytes with SHA-256
`7a206884b4cf029300173bee08d32a6f8b9c12224ad28a4364a29f16824e2a5f`;
the XIAO application is 1,425,792 bytes with SHA-256
`7ab81ba5620a6ae211e84c402e01058f457354dd98a97120a89c14693a759ad2`.
S3 read-back reproduced both hashes and confirmed AES256 encryption. Reviewed
replacement-free Change Set `webflash-rev17-20260907` modified only the existing
Lambda, integration and object-key-scoped role; the stack returned to
`UPDATE_COMPLETE`, and its parameters plus Lambda environment read back the
exact REV17 keys, sizes and hashes. Anonymous access returns 401. An existing
REV15 N16 grant was checked after activation and failed closed with
`authorized:false`. No firmware grant was migrated automatically; the separate
local-password-recovery grants are unaffected.

`C6-001-REV18-AWS` was activated for both targets on 2026-09-07. It adds the
retained `system/board` birth state and renames the V1 decoder identity to cover
both confirmed first-generation vehicle groups. The N16 application is
1,439,792 bytes with SHA-256
`fa9e029e383e7734d2f9b113395646573bf4d3e7ce9792ad701be6579ed71d1d`;
the XIAO application is 1,425,984 bytes with SHA-256
`9c031c7743a84d40fdae6d86e2dead27a8537c81525f3d22d9986a8cd6e11391`.
S3 download/read-back reproduced both hashes and confirmed AES256 encryption and
versioned objects. Reviewed Change Set `webflash-rev18-20260907` modified the
same three existing resources in place without replacement or deletion. Stack
parameters and Lambda environment read back the exact REV18 keys, sizes and
hashes; anonymous access returns 401 and an existing REV17 N16 grant fails closed
with `authorized:false`.

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

On 2026-09-16 the controlled N16 catalog entry advanced from REV21 to the exact
`C6-001-REV26-AWS` image built from commit `b427650`. The 1,476,224-byte private
artifact has SHA-256
`597c00565204d0531ec8b46d676071f3dc4dab1b4949a6ebe98fdcfaed0882d0` and S3
version `xEew5m0C70jSHcd8KQJu_B25t25ngr3b`; upload read-back reproduced the hash
and confirmed AES256 encryption. Reviewed Change Set
`webflash-rev26-20260916` modified only the existing Lambda, integration and IAM
role without replacement or deletion. The stack returned to `UPDATE_COMPLETE`,
Lambda read-back is `Active`/`Successful`, IAM permits only the exact XIAO REV21
and N16 REV26 objects, and anonymous API access remains `401`. The three active
N16 grants for Maya Eisele, Christian Reding and Elisabeth Hascouet were renewed
atomically for the REV26 version/hash with their existing expirations and three
successful audit events.

On 2026-09-16 the accepted ACCESS-001 release advanced the N16 catalog to
`C6-001-REV27-AWS`. The 1,477,344-byte artifact has SHA-256
`993b32c65904db00eda65166c58cea9577355baebde693db89ef9e5dde0bee4c` and private
AES256-encrypted S3 version `TBtGb9yKfGEiNncJIfPETi4O9ibPsQ.w`; download
read-back reproduced the exact hash. Reviewed Change Set
`webflash-rev27-20260916` again modified only the existing Lambda, integration
and IAM role without replacement. Stack read-back is `UPDATE_COMPLETE`, Lambda
is `Active`/`Successful`, and anonymous firmware access remains `401`. The same
three active N16 grants were renewed atomically for REV27 with unchanged
expirations and three successful audit events. XIAO remains unchanged on REV21.

### Web Serial reset/recovery follow-up

A macOS Chrome/Edge field report showed that starting WebFlash immediately takes
the running adapter offline. Entering the ESP32 ROM loader is required for chip
and flash identification and is therefore expected. The defect was that a failed
preflight, explicit disconnect or failure before a completed write only closed
the serial transport and could leave the adapter in the ROM loader or flasher
stub. The portal now tracks every bootloader session and resets back to the
application before closing the transport on all exit paths. The bundled
esptool-js 0.6.0 `hard_reset` only deasserts RTS and therefore produces no reset
edge when RTS is already inactive. The corrected path releases the boot strap,
asserts RTS/EN for 120 ms, deasserts it and allows 800 ms for boot before closing
USB. Error, disconnect and successful-write behavior then required physical
validation before the held pilot grants could be restored.

Physical retesting on 2026-09-17 passed for the corrected diagnostic path and the
complete WebFlash write. The first recovery attempt did not restart the
application: the LED remained green, but the adapter stayed off-network. The
success message now states that the restart was triggered and instructs the user
to power the adapter off and on if it has not returned to the network after 60
seconds. Two subsequent physical runs accepted the explicit DTR/RTS reset pulse and the
adapter returned to normal operation after WebFlash. The safety hold was lifted.
The newly built `C6-001-REV28-AWS` N16 application is 1,479,584 bytes with
SHA-256 `1f7e1432ba154cbccf0f0ddfab110034ddb72169395886d1247979e20c933dd4`.
Upload read-back reproduced the exact hash and confirmed AES256 encryption on S3
version `1fsXBRMpBZCqLif4PY6TIc_5M3mLMfCV`. Reviewed Change Set
`webflash-rev28-20260917` modified only the existing Lambda, integration and IAM
role with `Replacement: False`; the stack returned to `UPDATE_COMPLETE` and the
Lambda is `Active`/`Successful`. The maintainer, Maya Eisele, Christian Reding
and Elisabeth Hascouet received audited 48-hour grants for the exact REV28
version/hash. An older, independently revoked N16 grant was not reactivated.
Anonymous firmware access remains HTTP 401.

On 2026-09-17 the remotely distinguishable LED refinement advanced the N16
catalog to `C6-001-REV29-AWS`. The 1,479,632-byte application has SHA-256
`3879d337c88eb18c6b958ede018ebdf4894bc40045ee901e3e1a5a2d505b98a4`.
Upload read-back reproduced the exact hash and confirmed AES256 encryption on S3
version `q4WkhrOFo22Br9lsIaNOmNTzOXnZSdNa`. Reviewed Change Set
`webflash-rev29-20260917` modified only the existing Lambda, integration and IAM
role without replacement; stack and Lambda read-back are healthy. The maintainer,
Maya Eisele, Christian Reding and Elisabeth Hascouet received audited 48-hour
REV29 grants. The older independently revoked N16 grant remains revoked and
anonymous firmware access remains HTTP 401.

### Deferred bootloader entry

A subsequent production run showed that the former **connect and check** action
already called `detectChip()`. That necessarily reset the C6 into its ROM loader
and removed WiFi/AWS before the user pressed the separate firmware-update button.
Leaving the page at that point could therefore strand the adapter even though no
write had started. The portal now treats the first action as USB selection only:
it validates the browser-reported VID/PID but does not open the serial port,
toggle DTR/RTS or interrupt the running firmware. Download and SHA-256 validation
also occur while the adapter remains online. Only the explicit confirmed
**Firmware aktualisieren** action opens the transport and executes bootloader
entry, chip/flash preflight, write and the accepted reset pulse as one bounded
operation. The UI states this transition explicitly. Physical hosted acceptance
passed on 2026-09-17: diagnosis remained stable, USB selection left the adapter
reachable, it went offline only after the explicit flash start, and the completed
write returned it to normal network operation through the automatic reset pulse.

## Password-recovery extension

The repository now contains a separate administrator-approved local-password
recovery flow. It deliberately does not reuse a firmware release grant: an
administrator grants the exact portal principal the reserved
`local-admin-password` action for 1–168 hours, and may revoke it independently.
The authenticated portal checks that grant and records bounded authorization and
success/failure events before and after opening Web Serial.

The browser connects to the normally running adapter at 115200 baud and sends the
existing physical `admin recover` console command. Firmware replaces only the
`otaPass` value in NVS and returns the random replacement once over USB. The
browser displays that secret in the Web-Flasher UI, never writes it to the
technical/browser log and never sends it to AWS or audit data. After the user
confirms that the password has been saved, the UI removes it from the DOM. WiFi
profiles, CAN selection, service configuration, LittleFS AWS
credentials and vehicle assignment remain unchanged. Because the same command is
available to anyone with physical USB console access, USB hardware identification
is an operational aid rather than a security boundary; the server-side grant
controls only the supported portal workflow. The reviewed Change Set
`webflash-password-recovery-20260907` added exactly five JWT-authorized routes
and updated the existing Lambda/integration in place without replacement or
deletion; stack `mot-dev-onboarding` reached `UPDATE_COMPLETE` on 2026-09-07.
Anonymous access returns 401 and the production portal origin passes CORS
preflight. A controlled `xruser` grant/access/start/result/revoke smoke test
passed, ending with the test grant in `REVOKED` state. Productive portal
acceptance then passed on 2026-09-07: the authorized UI completed a real local
password replacement, showed the password above the saved/clear confirmation,
and the administrator successfully revoked the grant afterwards. Function and
GUI are accepted.

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
