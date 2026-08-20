# CAN Profile Framework

> **Status:** Current source-based architecture; hardware validation pending
>
> **Audience:** Firmware developer and vehicle-decoder contributor

## Purpose

The CAN profile framework separates physical CAN reception from vehicle-specific decoding. Firmware selects one profile, while telemetry, MQTT, AWS and the dashboard continue to consume the same canonical telemetry model.

```text
CAN frame -> selected DecoderProfile -> canonical Telemetry -> MQTT / ABRP / Web UI
```

## Profiles

| ID | Key | Status | Purpose |
|---:|---|---|---|
| `0` | `display-can` | active | Current Microlino Display-CAN decoder (`0x602`, `0x603`, `0x604`). |
| `2` | `standard-can-v1-pioneer` | active | Physically verified Pioneer `0x18D` decoder plus provisional `0x4AD` cell pair. Value `2` preserves stored configuration compatibility with the former generic template. |
| `3` | `standard-can-v2` | pilot | Independent V2 implementation with provisional `0x18D`/`0x4AD` rules pending validation on a V2 vehicle. |
| `255` | `disabled` | active | Disables decoding on a CAN input. |

Profile IDs are persistent configuration values. Existing value `2` remains the Standard-CAN selection. Unsupported or legacy values are normalized to the safe default (`display-can`).

## Source layout

- `firmware/common/decoders/decoder_profile.*`: registry, metadata and validation
- `firmware/common/decoders/decoder_engine.*`: profile dispatch
- `firmware/common/decoders/decoder_display_can.*`: production Display-CAN decoder
- `firmware/common/decoders/decoder_standard_can_bms.h`: shared scale-neutral BMS decode mechanism
- `firmware/common/decoders/decoder_standard_can_v1_pioneer.*`: Pioneer profile wrapper
- `firmware/common/decoders/decoder_standard_can_v2.*`: independent provisional V2 rules and profile wrapper

Both ESP32-WROOM and LilyGO call the same decoder engine with their persisted active profile.

## Adding a profile

1. Add a stable enum value to `DecoderProfile`.
2. Add one descriptor to `PROFILES` in `decoder_profile.cpp`.
3. Add a decoder implementation with no transport, MQTT or dashboard dependencies.
4. Add dispatch in `decoder_engine.cpp`.
5. Map decoded values only to the canonical `Telemetry` structure.
6. Add tests with recorded frames before marking the profile as implemented.

Do not guess production PIDs or scaling. A template profile must remain a no-op until its source data is authoritative.
