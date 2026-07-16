# AWS-3.4 Dashboard cleanup

The production Dashboard now labels its connection as **Cloud** instead of
MQTT and loads only the AWS REST provider.

Removed from the production `index.html`:

```text
libs/mqtt.min.js
js/providers/legacy-mqtt-provider.js
```

The files may remain in the repository for local debugging, but they are no
longer downloaded or executed by the AWS Dashboard.

`config.js` contains no broker username or password. A separate
`config.legacy-mqtt.example.js` documents the optional local debug mode.

The Cloud status means:

```text
Browser -> AWS Vehicle API
```

The Microlino/OBD2 status continues to come from the vehicle state stored in
AWS.
