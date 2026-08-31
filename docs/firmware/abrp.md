# ABRP integration

ABRP support is optional and builds telemetry for A Better Routeplanner.

## Payload fields

Depending on available valid telemetry, the payload may include:

- state of charge,
- Unix UTC timestamp,
- speed,
- power,
- charging state,
- latitude/longitude.

## Time requirements

ABRP requires a valid Unix timestamp. Earlier firmware logic considered timestamps above `1700000000` valid and did not send ABRP telemetry until time was valid.

## Transport status

| Transport | Status |
|---|---:|
| WiFi HTTP/HTTPS path | Implemented during earlier releases |
| LilyGO LTE HTTPS | Deferred |

The LTE implementation must be non-blocking enough that it does not degrade the AP/WebUI and must use a validated secure client path.

## C6 AWS coexistence and recovery

On the unified C6 image, ABRP can operate alongside MOT Cloud or in ABRP-only
mode. With MOT Cloud enabled and provisioned, ABRP queues a request only while
WiFi has a valid station address and AWS MQTT is connected. With MOT Cloud
disabled, or without provisioned AWS credentials, ABRP requires only valid WiFi.
Before creating a second TLS session it checks total free heap and the largest
free allocation block; a low-memory request is deferred rather than risking AWS,
WebUI or CAN/GPS service.

The ABRP HTTP/TLS objects are destroyed before their FreeRTOS sender task deletes
itself. This ordering is required: deleting the task while C++ objects are still
in its stack skips their destructors and previously leaked TLS allocations on
every request.

Authenticated WebUI configuration remains the normal control surface. USB serial
provides recovery and qualification commands without exposing or deleting stored
credentials:

- `abrp status`
- `abrp enable`
- `abrp disable`
- `abrp send`

Status JSON includes `heapBefore`, `heapAfter`, `largestFreeBlock` and
`lowMemorySkips` for bounded soak evidence.

ABRP resolves `is_charging` with the same source priority as AWS: fresh Pioneer
Standard-CAN pack status and current determine the value; the older Display-CAN
estimate is used only as a compatibility fallback when those BMS values are not
available or stale. This applies to the shared C6/WROOM sender and the maintained
LilyGO sender.

### REV4 N16 hardware evidence — 2026-08-15

The original failure was reproduced with AWS and ABRP enabled: free heap fell to
about 14.7 KB, ABRP reported `SSL - Memory allocation failed`, AWS reconnects
failed and the local WebUI timed out while both CAN loops continued. After the
destructor-order fix, 19 consecutive ABRP HTTPS requests returned HTTP 200 while
AWS remained connected. Across the final ten measured requests, `heapBefore`
remained approximately 231.35 KB and `heapAfter` approximately 222.2–222.3 KB;
the largest free block repeatedly recovered to 196.6 KB and no low-memory request
was recorded.
