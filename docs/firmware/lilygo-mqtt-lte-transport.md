# LilyGO MQTT WiFi/LTE Transport

Adds experimental AT-based LTE TCP transport for PubSubClient.

Transport selection:

```text
WiFi connected -> WiFiClient
else LTE GPRS  -> LilygoLteClient
else           -> MQTT disconnected
```

SIMCom commands used include `AT+NETOPEN`, `AT+CIPOPEN`, `AT+CIPSEND`, `AT+CIPRXGET`, and `AT+CIPCLOSE`.
