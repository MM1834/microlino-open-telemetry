> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO LTE CIPOPEN URC Fix

Fixes LTE MQTT connect behavior by making `lilygoLteTcpOpen()` wait for the SIMCom socket URC:

```text
+CIPOPEN: 0,0
```

The previous implementation could treat plain `OK` as a successful TCP connection. On SIMCom modules `OK` only means the command was accepted; the real socket result arrives later as `+CIPOPEN`.

## Expected effect

`LilygoLteClient::connect()` should return success only after the socket is actually open, allowing PubSubClient to proceed to the MQTT CONNECT write step.
