# LilyGO LTE Stack v3 Clean

A clean A7670 AT socket stack for MOT.

Known behavior on the tested modem firmware:

- `AT+CIPSTATUS` returns `ERROR`.
- `AT+CIPRXGET` returns `+IP ERROR: operation not supported`.
- `AT+CIPOPEN` may only expose `OK`, without a visible `+CIPOPEN` URC.

The stack uses internal socket state, manual `CIPSEND` prompt parsing, and receive URC parsing (`+RECEIVE` / `+IPD`) into a small buffer.
