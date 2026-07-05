# LilyGO MQTT GPS + Diagnostics

Adds GPS topics to WiFi MQTT publishing:

```text
location/latitude
location/longitude
location/speed_kmph
location/satellites
location/hdop
location/age_ms
```

Also improves MQTT diagnostics:

- state text for PubSubClient state
- last connect state
- connect attempt counter
- WiFi IP in MQTT status
- shorter socket timeout for failed broker connections

`state=-1` means PubSubClient connection timeout, usually host/port/firewall/broker listener mismatch.
