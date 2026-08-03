> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO Full LewisXhe A76XX LTE Transport

Why this exists:

- The official TinyGSM release used earlier did not include A7670/A76XX support.
- The tested obd2mqtt firmware works with `https://github.com/lewisxhe/TinyGSM` and `TINY_GSM_MODEM_A76XXSSL`.
- Custom SIMCom AT socket code failed because this modem firmware reports:
  - `AT+CIPSTATUS` -> `ERROR`
  - `AT+CIPRXGET` -> `+IP ERROR: operation not supported`

This patch replaces the MOT modem/TCP transport files with a shared LewisXhe TinyGSM modem/client stack.
