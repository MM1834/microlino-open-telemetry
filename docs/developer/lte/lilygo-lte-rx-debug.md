> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE RX Debug

Adds:

```text
GET /api/lilygo/lte/rx-debug
```

The endpoint queries `AT+CIPRXGET=4,0`, attempts one read through the current LTE receive function, and returns hex/ascii output.

Also fixes the warning caused by accidental `'\\r'` / `'\\n'` char constants.
