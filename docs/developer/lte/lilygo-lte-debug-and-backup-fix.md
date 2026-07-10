> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE Debug and Backup Fix

Changes:
- Slows MQTT reconnect attempts on LTE to 60 seconds.
- Adds `GET /api/lilygo/lte/debug`.
- Adds LTE debug link in the Web UI modem card where applicable.
- Ensures ABRP config is exported in config backup.

The longer LTE reconnect interval keeps the local AP/Web UI responsive while LTE TCP/MQTT is being debugged.
