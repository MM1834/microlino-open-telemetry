# Persistent Dashboard MQTT Client ID

The dashboard stores one stable MQTT client ID per browser profile, broker and vehicle in `localStorage`.

Example:

```text
mot-dashboard-9f52c438a1de
```

The ID is reused across reloads and browser restarts, preventing hundreds of random client entries in ioBroker.

Optional `config.js` settings:

```javascript
mqtt: {
  clientIdPrefix: 'mot-dashboard',
  // clientId: 'mot-dashboard-wall-tablet',
  // clientIdStorageKey: 'mot.dashboard.clientId'
}
```

To reset the generated ID, remove the matching `mot.mqttClientId.*` entry from browser localStorage.
