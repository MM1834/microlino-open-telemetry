> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE Stack v3 Clean

A clean A7670 AT socket stack for MOT.

Known behavior on the tested modem firmware:

- `AT+CIPSTATUS` returns `ERROR`.
- `AT+CIPRXGET` returns `+IP ERROR: operation not supported`.
- `AT+CIPOPEN` may only expose `OK`, without a visible `+CIPOPEN` URC.

The stack uses internal socket state, manual `CIPSEND` prompt parsing, and receive URC parsing (`+RECEIVE` / `+IPD`) into a small buffer.
