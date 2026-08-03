# Configurable vehicle status timeouts

The Microlino/OBD2 presence thresholds now live in `dashboard/config.js`:

```javascript
dashboard: {
  vehicleOnlineSeconds: 120,
  vehicleStaleSeconds: 600
}
```

Meaning:

- age <= `vehicleOnlineSeconds`: Online
- age <= `vehicleStaleSeconds`: Data stale
- older: Offline

`vehicleStaleSeconds` is automatically clamped so it cannot be shorter than
`vehicleOnlineSeconds`.

The same settings are documented in `config.example.js`.
