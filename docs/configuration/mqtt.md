# MQTT Configuration

## Fields

- MQTT host
- MQTT port
- MQTT user
- MQTT password
- MQTT prefix
- Vehicle ID

## Topic structure

```text
<prefix>/<vehicleId>/<category>/<name>
```

Example:

```text
mot/pioneer/display/soc
mot/pioneer/location/latitude
mot/pioneer/system/network_mode
```

## LilyGO transport

LilyGO supports:

- WiFi MQTT
- LTE MQTT transport: experimental

WiFi remains the stable MQTT path. LTE MQTT requires field testing with a broker reachable over the mobile network.
