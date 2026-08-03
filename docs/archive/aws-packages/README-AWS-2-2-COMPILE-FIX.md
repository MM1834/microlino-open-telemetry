# AWS-2.2 compile fix

Adds the missing forward declaration:

```cpp
static String mqttClientId();
```

`publishBirthMessages()` calls `mqttClientId()` before its implementation later
in the file, so the compiler needs this declaration.
