> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO Modem Boot Recovery

Adds a staged modem recovery path for cases where ESP32 reset/OTA leaves the A7670 modem not answering `AT`.

Recovery sequence:

```text
1. Normal AT wait
2. UART re-init + AT retry
3. Soft cleanup: CIPCLOSE, NETCLOSE, CFUN=0, CFUN=1
4. Power-key pulse + AT retry
5. Report AT failed if still unavailable
```
