# MOT Dashboard v1.0 responsive refresh

This package replaces the dashboard with a cleaner responsive layout for desktop, tablet and mobile.

## Install

Copy the contents into the repository root:

```bash
unzip mot-dashboard-v1.0-responsive.zip
cp -R mot-dashboard-v1.0-responsive/. .
rm -rf mot-dashboard-v1.0-responsive
```

Then replace:

```text
dashboard/libs/mqtt.min.js
```

with the working MQTT.js browser build already used in your deployment.

## Configuration

Edit `dashboard/config.js` locally. The repository should keep neutral defaults only.

Important values:

```js
host: "mqtt.example.com",
port: 443,
useTls: true,
path: "/",
topicPrefix: "mot",
vehicleId: "pioneer"
```

The dashboard subscribes to:

```text
mot/<vehicleId>/#
```
