# Pioneer Standard-CAN decoder

> **Status:** Pack voltage, current scale, driving sign, traction/regeneration and
> stable plug/charge states physically confirmed on Microlino Pioneer; cell-pair
> semantics remain provisional

## Confirmed `0x18D` layout

Only standard frames with DLC 8 are accepted.

| Bytes | Type and scale | MOT value | Status |
|---|---|---|---|
| `data[1..2]` | signed little-endian × 0.3 A | pack current | Confirmed for idle, charging, traction and regeneration |
| `data[3..4]` | unsigned little-endian mV | pack voltage | Confirmed |
| `data[6]` | `0x10` unplugged, `0x20` plugged | stable plug state | Confirmed; `0x04` is transitional |
| `data[7]` | source-specific byte | Standard-CAN SOC field | Not Pioneer display SOC; must not replace Display-CAN SOC |

## Confirmed Pioneer `0x48D` SOC fields

Controlled driving, rest and charging runs on 2026-09-03 established two separate
whole-percent values in the Pioneer-only `0x48D` frame:

| Byte | MOT value | Evidence status |
|---|---|---|
| `data[6]` | `bms/soc_internal` | Internally transmitted SOC-like value; meaning remains provisional |
| `data[7]` | `bms/soc_display` | Confirmed to follow the visible Pioneer SOC exactly |

The internal value changed independently and remained about 6–8 percentage points
below the display value during the controlled run. It must not be described as a
true SOC until its BMS semantics are independently confirmed. `bms/soc_display`
is diagnostic corroboration only; `display/soc` remains the product SOC source.
The large-battery V2 capture contained no `0x48D` frames and instead exposes its
SOC/SOH pair on `0x1B1`, so this decoding is deliberately confined to Pioneer V1.

Pack power is derived as `current_A × voltage_V`. This field uses the battery
convention: positive current/power flows into the pack during charging or
regeneration; negative current/power leaves the pack during traction. MOT also
derives `vehiclePowerW = -packPowerW`, preserving the existing portal, History and
ABRP convention where consumption is positive and charge/regeneration is negative.

Active charging is derived only when the stable plug state is `0x20` and current
exceeds 2.0 A. The observed idle offset of -3/-4 raw units is therefore not
classified as charging or meaningful discharge.

## Controlled charge evidence

| Test state | External reference | Raw current | Decoded current | Pack voltage | Derived DC power |
|---|---:|---:|---:|---:|---:|
| Plugged, EVSE unpowered | 0 W | -4 | -1.2 A | 49.600–49.680 V | idle offset |
| Mobile EVSE setting 6 A | 1,809 W AC input | 118–121 | 35.4–36.3 A | 49.920–50.059 V | 1.77–1.82 kW |
| Mobile EVSE setting 8 A | 2,088 W AC input | 135–139 | 40.5–41.7 A | 50.140–50.280 V | 2.03–2.10 kW |
| Second EVSE setting 10 A | 2,500 W charger display | 141–166 | 42.3–49.8 A | 50.300–50.420 V | 2.13–2.51 kW |
| Second EVSE setting 16 A, initial | briefly 3,200 W display | 165–167 | 49.5–50.1 A | 50.420–50.539 V | 2.50–2.53 kW during capture |
| Same session after regulation | about 2,340 W display | minimum 155 | 46.5 A | about 50.51 V | about 2,349 W |
| Failed charge start, cable retained | error / no energy flow | -4 | -1.2 A | 49.899–50.020 V | idle offset |

The independently observed 2,340 W regulated point and 2,349 W CAN-derived point
differed by about 9 W. Together with the lower-power points this is sufficient to
treat the 0.3 A/raw-unit charging scale as confirmed for Pioneer.

## Plausibility boundary

The production decoder rejects rather than publishes:

- pack voltage outside 40–65 V;
- charge/regeneration samples above 12 kW;
- discharge samples above 25 kW;
- candidate cell values outside 2.0–5.0 V.

Rejected-sample counters remain available in firmware diagnostics. These bounds
protect AWS and portal consumers from start/shutdown transients and bound the
physically observed road-test range.

The bounds became asymmetric after the first normal-road AWS run. That drive
reached a coherent -450 A candidate with voltage sag to 46.379 V, approximately
20.9 kW leaving the pack, while speed reached 82.5 km/h. The original symmetric
15 kW guard rejected 324 otherwise coherent high-load samples. A 25 kW discharge
ceiling retains margin around this physical evidence; the 12 kW positive ceiling
remains deliberately tighter because confirmed charge/regeneration peaks are much
lower.

## Controlled flat-road evidence

Three synchronized dual-CAN runs on 2026-08-06 correlated Standard-CAN current
with Display-CAN speed. Moderate and stronger acceleration produced negative
current while accelerator release produced an immediate positive-current
transition. The focused run reached -260.4 A at approximately 48.64 V, about
12.7 kW leaving the pack, and +94.5 A, about 4.8 kW returned during pedal-release
regeneration. Constant travel near 52.5 km/h required approximately 2.2–3.7 kW.

A separate comparison from similar speed distinguished pedal-release from light
braking. Pedal release produced about 4.5 kW peak regeneration; light braking
raised electrical regeneration to +155.1 A at 50.60 V, about 7.8 kW. This confirms
that the Pioneer increases regenerative braking when the brake pedal is applied.
No maximum-acceleration or emergency-braking run was required.

## `0x4AD` candidate pair

`data[0..1]` and `data[2..3]` decode as two little-endian millivolt values, with
derived min/max/delta. Values around 4.08–4.11 V followed charging plausibly, but
the exact meaning and whether they are true full-pack cell extrema still require
independent confirmation. MOT publishes them with this provisional status.

## Cloud contract

The shared WROOM, LilyGO and C6 publishers use:

| Topic suffix | Unit |
|---|---|
| `bms/pack_voltage` | V |
| `bms/pack_current` | A |
| `bms/pack_power_w` | W |
| `bms/vehicle_power_w` | W |
| `bms/is_regenerating` | boolean |
| `bms/is_discharging` | boolean |
| `bms/status_byte` | raw integer |
| `bms/cell_min_mv` | mV |
| `bms/cell_max_mv` | mV |
| `bms/cell_delta_mv` | mV |
| `bms/soc_internal` | whole percent; Pioneer-only candidate |
| `bms/soc_display` | whole percent; Pioneer display-correlated |
| `charging/plugged` | boolean |
| `charging/is_charging` | boolean |
| `charging/power_signed` | signed 0.1 kW compatibility value; consumption positive, charge/regen negative |

The AWS state ingestion path accepts these topics generically. The portal renders
pack voltage, current, power and the provisional cell extrema. Repository support
does not itself prove a deployed C6 connection or hosted-portal update.
