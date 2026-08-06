# C6 Pioneer traction and regeneration road test

> **Status:** Executed on 2026-08-06; traction, regeneration and light-brake
> regeneration confirmed

## Purpose

Correlate the confirmed `0x18D` battery-current field with traction and
regeneration on a flat, legal, low-traffic road. Confirm its driving sign,
plausible peak range and relationship to Display-CAN speed and power indication.

## Safety boundary

- The driver does not operate the MacBook or serial console while moving.
- Normal traffic rules, visibility and other road users always override the test.
- No emergency braking, maximum-speed run or deliberately abrupt manoeuvre is
  required.
- Both CAN controllers remain listen-only.

## Preparation

1. Connect CAN1 to Pioneer Standard CAN and CAN2 to Display CAN.
2. Power the C6 continuously by USB.
3. Confirm both frame counters advance with zero errors.
4. Issue `drive reset` immediately before departure.
5. Note displayed SOC and whether HVAC or other major consumers are active.

The C6 keeps 600 synchronized samples at 200 ms intervals, retaining the most
recent 120 seconds in RAM. Each sample contains elapsed time, Display-CAN speed,
Standard-CAN current/voltage/status, derived power, Display-CAN power byte and a
plausibility flag.

## Suggested flat-road sequence

Perform only where safely possible:

1. Standstill for approximately 5 seconds.
2. Moderate acceleration to approximately 30 km/h.
3. Hold approximately 30 km/h for 10 seconds.
4. Release the accelerator completely and coast/regenerate without braking.
5. Gently brake to standstill and wait 5 seconds.
6. Moderate acceleration to approximately 50 km/h.
7. Hold approximately 50 km/h for 10 seconds.
8. Release the accelerator, then gently brake to standstill.

A return run in the opposite direction helps cancel a small gradient or wind
bias. It should be recorded as a separate `drive reset` window where practical.

## Collection

Keep USB power connected after stopping. Collect:

```text
drive dump
drive trace
```

The trace is lost on reset or power removal. Analysis should establish:

- current sign during acceleration and pedal release;
- current near constant 30/50 km/h on the same road;
- regenerative peak and decay;
- voltage sag/rise under traction/regen;
- rejection frequency at the configured plausibility boundaries;
- correlation, if any, with `0x37F data[2..3]` and Display `0x603 data[4]`.

Do not promote consumption or regeneration energy to a confirmed portal metric
until this correlation passes. Trip energy integration can then use the confirmed
pack watts over time, with separate positive traction and negative regeneration
totals.

## Results

All three runs received both CAN buses with zero reported errors. Display-CAN SOC
was 69–71%; Standard-CAN `0x18D data[7]` remained 90 and is therefore not Pioneer
SOC. The road-test extrema and correlations were:

| Observation | Result |
|---|---:|
| Maximum observed traction current | -260.4 A |
| Maximum observed traction power | about 12.7 kW leaving battery |
| Pedal-release regeneration | up to about 4.8 kW into battery |
| Light-brake regeneration | up to +155.1 A / about 7.8 kW into battery |
| Constant 52.5 km/h on test section | about 2.2–3.7 kW consumption |
| Maximum observed speed | 53.5 km/h |

The sign and 0.3 A/raw-unit scale are confirmed across charging, traction and
regeneration. The firmware battery convention is positive into the pack; the
derived vehicle convention negates this value for consumption-positive portal,
History and ABRP consumers. A Sport-mode maximum-acceleration run was deliberately
not performed because it would not materially improve decoder confidence.

## Normal-road AWS follow-up

A subsequent approximately 12.7-minute home drive kept WiFi, AWS IoT and both CAN
channels connected without CAN errors and delivered the new BMS fields to the
AWS state table. Display SOC fell from 66% to 60% and speed reached 82.5 km/h.
The full-window summary observed -450.0 A with pack voltage as low as 46.379 V
(about 20.9 kW discharge) and +175.5 A regeneration. This exposed the original
15 kW symmetric plausibility guard as too restrictive: 324 high-load samples were
rejected. The decoder now uses separate 25 kW discharge and 12 kW charge/regen
ceilings.
