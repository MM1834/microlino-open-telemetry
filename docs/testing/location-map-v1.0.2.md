# Location Map – v1.0.2

The dashboard uses the configured default location until live GPS MQTT topics are available.

## Behaviour

- If MQTT GPS topics are available, the map uses live GPS.
- If no GPS topics are available and `defaultLocation.enabled` is true, the map uses the configured default location.
- The dashboard embeds an OpenStreetMap view at street-level zoom.
