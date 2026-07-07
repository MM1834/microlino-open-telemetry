# LilyGO LTE CIPRXGET Receive Fix

Replaces the LTE socket receive functions with SIMCom `AT+CIPRXGET` based logic.

Target symptom:

```text
writeCalls > 0
availableCalls high
lastAvailable = 0
readCalls = 0
```

Test after upload:

```text
/api/lilygo/lte/mqtt-trace
/api/lilygo/mqtt
```
