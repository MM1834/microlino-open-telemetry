# SPR-0005 ESP32-WROOM AWS Build Evidence

> **Status:** Current compile evidence; no hardware or cloud validation
>
> **Audience:** Firmware maintainer and beta release reviewer
>
> **Validated commit:** `5758191`
>
> **Date:** 2026-08-02

## Boundary

Two clean compile-only builds were run for `esp32dev-aws`. No `upload`, `uploadfs`,
`buildfs`, `erase`, serial monitor, device access or AWS action was performed.

## Toolchain

| Component | Version |
|---|---|
| PlatformIO Core | 6.1.19 |
| Espressif32 platform | 6.13.0 |
| Arduino ESP32 framework | 3.20017.241212 (`dcc1105b`) |
| Xtensa toolchain | 8.4.0+2021r2-patch5 |
| Environment | `esp32dev-aws` |
| Firmware-visible version | `SPR-0004B.9-REV2-AWS` |

## Result

Both clean builds passed.

| Metric | Result |
|---|---:|
| RAM | 50,864 / 327,680 bytes (15.5%) |
| Program flash | 1,140,957 / 1,310,720 bytes (87.0%) |
| `firmware.bin` size | 1,147,536 bytes |
| First SHA-256 | `6a4591f3ccefb1fee59037580eeea7a57ae424e987f66d1ff5a41f42a2da9f67` |
| Second SHA-256 | `6094ea64d8540334039c936b25607367a1ab2ce80a3bcf87356d6f0ca5db0e55` |

## Findings

The environment is compile-valid, but the two clean artifacts are not bit-for-bit
reproducible. Current version metadata includes compiler-provided build date/time,
which is a likely source and must be isolated before claiming deterministic builds.

Flash use leaves approximately 13% of the application partition. ONB-001 does not
run on the device, but future firmware security/support additions and OTA strategy
must track this margin.

One AWS artifact covers GPS and no-GPS hardware. Optional GPS code is included in
the production build; physical validation still requires detected/fix and absent-
hardware cases.

## Next evidence

- identify all time/path/tool inputs causing binary differences;
- decide whether deterministic binary output is a release requirement or whether
  an exact stored artifact plus hash is the controlled-release model;
- run physical-device validation with the selected stored artifact;
- do not interpret this compile result as AWS X.509 or runtime validation.
