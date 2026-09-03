# Bounded Telemetry Debug Capture

> **Status:** Deployed; first bounded `xrpioneer2` capture active
>
> **Audience:** Maintainer, administrator and diagnostic recipient
>
> **Last reviewed:** 2026-09-01

## Purpose and boundary

The debug capture preserves short-lived, high-cadence scalar telemetry for one
explicitly approved vehicle when the normal minute-aggregated History is
insufficient for fault diagnosis. It does not change firmware publication cadence
or vehicle behaviour. The existing AWS IoT messages and State-ingest Lambda are
reused.

The capture accepts numeric and boolean values below `display/`, `charging/` and
`bms/`. This includes future confirmed BMS topics without persisting arbitrary
objects. Location, system, status, credentials and other namespaces are excluded.

## SOC source boundary

`display/soc` is decoded from the Microlino Display-CAN port and is the only SOC
value published to MOT Cloud and stored by normal or debug History. It must not be
described as BMS SOC.

The observed Standard-CAN/BMS SOC candidate is intentionally not published. Its
semantics could not be mapped reliably to the vehicle's displayed SOC and it must
not replace or be merged with `display/soc`. If Microlino later confirms a separate
Standard-CAN SOC PID and scaling, it requires an explicitly named `bms/*` topic and
separate validation before cloud publication.

## Fail-closed controls

All of the following must be true for a write:

- `EnableTelemetryDebug=true`;
- the exact vehicle ID appears in `TelemetryDebugVehicleAllowlist`;
- `TelemetryDebugCaptureUntilEpochSeconds` is non-zero and has not passed;
- the message receipt timestamp is not later than the deadline;
- the topic namespace and scalar type are allowed.

The default configuration is disabled, empty and expired. The absolute deadline
stops writes without a second deployment. Debug rows use a separate encrypted,
on-demand DynamoDB table and expire through TTL after seven days by default.
Normal State, History, WebSocket and notification behaviour remain independent.
The deadline is not limited to 48 hours; 48 hours was the conservative first
activation window. A later parameter-only update may set another explicit UTC
deadline without changing the seven-day per-row retention.

## Data model

Table: `${ProjectName}-${Environment}-vehicle-debug-history`.

| Attribute | Meaning |
|---|---|
| `vehicleId` (PK) | Exact debug-approved vehicle identity |
| `sampleKey` (SK) | `<13-digit receivedAt ms>#<topic suffix>`; chronological within the vehicle |
| `topicSuffix` | Original MQTT suffix |
| `sampledAt` / `receivedAt` | AWS IoT receipt epoch in milliseconds |
| `value` / `valueType` | Decoded scalar value and type |
| `expiresAt` | DynamoDB TTL in epoch seconds |

The dedicated daily-write alarm defaults to 250,000 writes. At a five-second
publisher interval this permits a bounded single-vehicle diagnostic session while
keeping the ordinary History alarm meaningful.

## Export

Use `tools/aws/export_vehicle_debug_history.py` with an exact vehicle ID, bounded
ISO-8601 start/end timestamps and a private output path. Exports contain telemetry
and must not be committed to the public repository.

## Operational closeout

After the deadline, confirm that new rows stop, export the required evidence and
retain it only for the diagnostic purpose. TTL deletion is asynchronous. For an
early stop, deploy the same stack with the debug flag disabled, empty allowlist and
deadline zero.
