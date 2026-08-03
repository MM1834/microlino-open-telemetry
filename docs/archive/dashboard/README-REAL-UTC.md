# Real UTC last-seen firmware patch

Publishes retained Unix UTC seconds on:

```text
<prefix>/<vehicleId>/system/last_seen_utc
```

Example:

```text
1784061797
```

## Time source

- ESP32-WROOM: SNTP/NTP after WiFi connects.
- LilyGO: SNTP/NTP when WiFi is available.
- LilyGO LTE-only: publishing waits until the ESP32 system clock has been set
  by a future modem/GSM or GPS time provider. It never overwrites the retained
  topic with uptime seconds or an invalid epoch.

The dashboard accepts Unix seconds and will display the local clock time.
