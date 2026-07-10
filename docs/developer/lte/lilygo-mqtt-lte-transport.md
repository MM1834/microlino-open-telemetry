> **Historical implementation note**  
> This document describes a specific development step. It may be superseded by the current firmware. Use it for investigation history, not as the primary user guide.

# LilyGO MQTT WiFi/LTE Transport

Adds experimental AT-based LTE TCP transport for PubSubClient.

Transport selection:

```text
WiFi connected -> WiFiClient
else LTE GPRS  -> LilygoLteClient
else           -> MQTT disconnected
```

SIMCom commands used include `AT+NETOPEN`, `AT+CIPOPEN`, `AT+CIPSEND`, `AT+CIPRXGET`, and `AT+CIPCLOSE`.
