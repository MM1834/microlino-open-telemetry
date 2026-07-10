> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO CAN SN65HVD230

Adds basic TWAI/CAN receive support:

```text
CAN RX GPIO32
CAN TX GPIO13
bitrate 500 kbit/s
```

Endpoints:

```text
GET /api/lilygo/can
GET /api/lilygo/can/frames
```

This sprint reads CAN frames and exposes counters/last frames. It does not transmit application CAN frames.
