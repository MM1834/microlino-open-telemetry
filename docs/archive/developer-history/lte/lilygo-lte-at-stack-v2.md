> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE AT Stack v2

Keeps the custom A7670 AT transport but avoids `AT+CIPSTATUS`, which returns `ERROR` on this firmware. It uses internal socket state, manual `CIPSEND` prompt parsing, and the URC RX buffer for incoming data.
