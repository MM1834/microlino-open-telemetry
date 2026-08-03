> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE MQTT Trace

Adds tracing around the Arduino `Client` implementation used by PubSubClient for LTE MQTT.

Endpoints:

```text
GET  /api/lilygo/lte/mqtt-trace
POST /api/lilygo/lte/mqtt-trace/clear
```

Use after LTE TCP test succeeds but MQTT still fails.
