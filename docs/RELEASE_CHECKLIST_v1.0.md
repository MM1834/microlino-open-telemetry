# MOT v1.0 Release Checklist

## 1. Remove private configuration

Before tagging a public release, make sure no private hosts, IP addresses, usernames, passwords, API keys or tokens are committed.

Recommended dashboard workflow:

```bash
cp dashboard/config.example.js dashboard/config.js
```

Then adapt `dashboard/config.js` only on the deployed web server.

## 2. Verify firmware topics

The firmware should publish under:

```text
mot/<vehicleId>/display/soc
mot/<vehicleId>/display/speed_kmh
mot/<vehicleId>/display/odometer_km
mot/<vehicleId>/charging/is_charging
mot/<vehicleId>/system/device_id
```

Default values:

```text
MQTT Prefix: mot
Vehicle ID: microlino
Vehicle Name: Microlino
```

For Martin's Pioneer test vehicle, the private deployment may use:

```text
Vehicle ID: pioneer
Vehicle Name: Microlino Pioneer
```

## 3. Verify dashboard

Open the dashboard and confirm:

- MQTT status becomes Online
- SOC updates
- speed updates
- odometer updates
- charging state updates
- firmware/device information appears

If the page is served over HTTPS, the MQTT WebSocket must use WSS.

## 4. Commit release preparation

```bash
git checkout develop
git add .
git commit -m "Prepare v1.0 release candidate"
git push
```

## 5. Create release candidate tag

```bash
git tag v1.0.0-rc1
git push origin v1.0.0-rc1
```

Create a GitHub pre-release from this tag.

## 6. Final release

After a final smoke test:

```bash
git checkout main
git merge develop
git tag v1.0.0
git push origin main
git push origin v1.0.0
```

Then publish the GitHub release.
