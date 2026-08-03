# Firmware and Hardware Gap Register

> **Status:** Current static-review findings; runtime impact unverified
>
> **Audience:** Maintainer, firmware developer and hardware reviewer

| ID | Finding | Evidence/impact | Next decision or validation |
|---|---|---|---|
| FW-001 | Too many maintained-looking PlatformIO environments | AWS, pre-AWS and GPS-test environments expose implementation history as product variants | Simplify after DOC-001; preserve one line per board |
| FW-002 | Open local AP and unauthenticated WebUI | WROOM remediation is implemented and build-tested in FW-SEC-001: controlled first setup only, then WPA2 plus authenticated WebUI/API. LilyGO remains open and unremediated | Hardware-validate WROOM; port and validate the same boundary on LilyGO before external handoff |
| FW-003 | Standard-CAN profile is empty | Registered profile deliberately decodes nothing | Obtain official identifiers/scaling before other models |
| FW-004 | ESP32 wiring was wrong in prior docs | Source declares RX27/TX26, old page showed LilyGO RX32/TX13 | Physically review every beta harness |
| FW-005 | LilyGO AWS transport is WiFi-only | AWS branch gates shared client on `WiFi.status()` | Keep hotspot path for now; design LTE/TLS separately |
| FW-006 | Legacy LTE MQTT path remains experimental | Non-AWS branch selects LTE client, with extensive historical fixes/traces | Revalidate bidirectional MQTT, reconnect, watchdog and power |
| FW-007 | Local route parity differs by board | OTA paths, status/diagnostics and form-save routes differ | Do not promise a single identical local API |
| FW-008 | MQTT payloads differ by transport | Legacy booleans `1/0`, AWS booleans `true/false`; Lily legacy topic set differs | Normalize or version contract in later implementation |
| FW-009 | Display-CAN values include assumptions | Fixed 140 km range and charging threshold/scaling in decoder | Calibrate with verified vehicle traces |
| FW-010 | Board header contains provider-specific APN default | Swisscom APN is compiled as default for LilyGO | Make beta provisioning explicit and provider-safe |
| FW-011 | Source comments contradict active LilyGO CAN code | Header calls CAN planned/disabled, while `main.cpp` initializes RX32/TX13 CAN | Correct code comments after hardware review |
| FW-012 | Current-head validation absent | No DOC-001 builds or device tests performed | Separate approved build/hardware validation phase |
| FW-013 | AWS readiness can be a false positive | ESP32 readiness marks AWS configured from the build flag, not successful credential loading or X.509 connection | Add explicit credential/connection states before relying on readiness |
| FW-014 | Factory reset retains AWS identity | Reset clears Preferences/NVS but not LittleFS certificate/key files | Define secure deprovision, ownership transfer and certificate revocation |
| FW-015 | Local OTA protection is configuration-sensitive | WROOM now denies OTA unless explicitly enabled, locally authenticated and same-origin; LilyGO still permits upload when its OTA password is empty | Hardware-validate WROOM and port fail-closed behaviour to LilyGO |
| FW-016 | System Health does not validate AWS transport | MQTT diagnostic uses legacy host credentials and may expose network/location fields | Add AWS-specific, privacy-safe connection diagnostics |
| FW-017 | Two ESP32 version headers remain | Shared `common/system/version.h` declares the active SPR build while board-local `include/version.h` still says `1.0.3-hotfix` | Remove or clearly retire the unused version source after build verification |
| FW-018 | Clean WROOM AWS builds are not bit-identical | Two successful `esp32dev-aws` builds at `5758191` produced different SHA-256 hashes; build date/time is compiled into metadata | Isolate nondeterministic inputs or adopt a controlled stored-artifact model |
| FW-019 | WROOM AWS application partition is 87% used | FW-SEC-001 build on 2026-08-03 uses 1,146,065 of 1,310,720 bytes (87.4%); RAM uses 50,864 of 327,680 bytes (15.5%) | Keep WROOM changes compact, verify local OTA with the release binary and define a hard release margin before further growth |

## Beta blockers

Before ESP32-WROOM beta handoff, the WROOM portions of FW-002/FW-015 require
hardware validation; FW-004, FW-009, FW-012 through FW-014, FW-016, FW-018 and FW-019
require an explicit review, correction or bounded acceptance. LilyGO beta use
additionally requires FW-005/FW-006 validation.

## Related documents

- [Firmware overview](overview.md)
- [Hardware comparison](../hardware/comparison.md)
- [Work order](../governance/WORK_ORDER.md)
