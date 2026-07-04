# ABRP Time Handling

ABRP requires `utc` to be a valid Unix timestamp in seconds. It does not accept a missing or invalid `utc` field.

## Firmware behaviour

MOT v1.0.3:

- starts NTP after WiFi connection,
- checks whether system time is valid,
- does not send ABRP telemetry until time is valid,
- always includes `utc` when sending to ABRP,
- does not send `lat`/`lon` until a real GPS/GNSS source is available.

## Valid time check

```cpp
static bool validUnixTime(time_t t)
{
    return t > 1700000000;
}
```

If time is not valid, ABRP status reports:

```text
ABRP waiting for valid system time (NTP)
```

## Payload

```json
{
  "soc": 56.0,
  "utc": 1750000000,
  "speed": 0.0,
  "power": 88.00,
  "is_charging": true
}
```
