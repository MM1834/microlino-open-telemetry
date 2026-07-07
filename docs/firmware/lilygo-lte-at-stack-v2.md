# LilyGO LTE AT Stack v2

Keeps the custom A7670 AT transport but avoids `AT+CIPSTATUS`, which returns `ERROR` on this firmware. It uses internal socket state, manual `CIPSEND` prompt parsing, and the URC RX buffer for incoming data.
