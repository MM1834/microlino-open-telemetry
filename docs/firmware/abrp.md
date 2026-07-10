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
