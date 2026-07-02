# Quick release guide

## 1. Apply the package

From the repository root:

```bash
unzip ~/Downloads/mot-release-v1.0-package.zip
cp -R mot-release-v1.0-package/. .
rm -rf mot-release-v1.0-package
```

## 2. Restore MQTT.js

Copy your working MQTT.js browser build to:

```text
dashboard/libs/mqtt.min.js
```

## 3. Check dashboard config

Edit:

```text
dashboard/config.js
```

Recommended production values:

```js
host: "mmds.muehlberg.ch",
port: 20226,
useTls: true,
topicPrefix: "mot",
vehicleId: "pioneer"
```

## 4. Build firmware

```bash
cd firmware/esp32-wroom
pio run
```

If successful:

```bash
pio run -t upload
pio device monitor
```

## 5. Firmware Web UI values

Set in the firmware Web UI:

```text
MQTT Prefix: mot
Vehicle ID: pioneer
Vehicle Name: Microlino Pioneer
```

Save and reboot.

## 6. Verify MQTT

Subscribe externally or internally to:

```text
mot/pioneer/#
```

Expected values include:

```text
mot/pioneer/display/soc
mot/pioneer/display/odometer_km
mot/pioneer/charging/is_charging
```

## 7. Upload dashboard

Upload the complete `dashboard/` folder to your webhoster, for example:

```text
https://www.muehlberg.ch/MOT/dashboard/
```

## 8. Commit and tag

```bash
git add .
git commit -m "Release v1.0.0"
git push

git tag v1.0.0
git push origin v1.0.0
```

Then create a GitHub release using `GITHUB_RELEASE_NOTES.md`.
