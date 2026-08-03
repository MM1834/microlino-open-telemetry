# CAN development

> **Status:** Current development guidance; new decoders remain planned
>
> **Audience:** Firmware and vehicle-decoder developer

The decoder converts TWAI frames into the shared telemetry model. New decoders should:

- avoid direct MQTT publishing,
- update normalized telemetry fields,
- preserve receive-only behavior until transmission is explicitly reviewed,
- add diagnostics and tests for new frame mappings.
