# AWS-3.6 PubSubClient include-order fix

`mqttStateText()` uses the `MQTT_*` constants before `MotAwsIot.h` is included.

The fix adds:

```cpp
#include <PubSubClient.h>
```

near the top of `lilygo_mqtt.cpp`, before `mqttStateText()` is compiled.
