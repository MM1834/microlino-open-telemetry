# Node-RED Legacy MQTT to AWS IoT Forwarder

> **Status:** Direct ioBroker-state forwarding deployed and validated
>
> **Source identity:** ioBroker `mqtt.3` / MQTT `mot/xrpioneer/#`
>
> **Target identity:** AWS IoT `mot/xrpioneer/#`

## Purpose

This flow forwards telemetry from the first-generation ESP32 through its existing
`mqtt.3` ioBroker objects into the current AWS pipeline without changing the old
firmware. Node-RED subscribes directly to the allowlisted ioBroker states instead
of opening a second connection to the local MQTT broker. It uses a dedicated AWS
IoT identity and does not expose the unencrypted broker to AWS.

Import [the flow template](../../tools/node-red/mot-xrpioneer-aws-forwarder.json)
through Node-RED's **Import → Clipboard/File** dialog. Keep the flow disabled until
the AWS broker configuration node is complete.

## Forwarded topics

Only the supplied `mot/xrpioneer` suffixes are allowed. Unknown topics are blocked
and reported to the debug sidebar. Boolean `0/1` payloads for
`charging/is_charging` and `charging/plugged` become JSON booleans. Numeric topics
become JSON numbers; bounded system values remain strings.
`charging/power_signed` is forwarded as a signed numeric value for the bounded
Power-History comparison with SOC and Speed.

Each direct state input emits the current value at flow start and then every
subsequent ioBroker state event. AWS publications remain retained so a newly
connected portal can obtain current state immediately.

## Direct ioBroker inputs

The flow contains one `ioBroker in` node for each allowed
`mqtt.3.mot.xrpioneer.*` state. Each node emits `payload`, sends all state events,
fires once at start and formats its object ID as an MQTT-style topic. The function
accepts both dot and slash topic formats before mapping to `mot/xrpioneer/...`.

This direct path replaced the local MQTT subscriber on 2026-08-05 after the
`mqtt.3` server interface repeatedly delivered only a restart snapshot and then
stopped answering subscriber connections while ioBroker objects continued to
update. Two successive AWS reads confirmed changing SOC, Speed and power values
after the replacement.

## AWS broker node

The dedicated Thing `mot-node-red-xrpioneer`, certificate and publish-only policy
`mot-node-red-xrpioneer-policy` were created on 2026-08-04. In **AWS IoT xrpioneer
· CONFIGURE TLS**, use the preconfigured ATS endpoint, port 8883 and the
unique client ID `mot-node-red-xrpioneer`. Select in the TLS node:

- `AmazonRootCA1.pem` as CA;
- the dedicated device certificate;
- the dedicated private key;
- server-certificate verification enabled.

Store these files with permissions limited to the Node-RED service account. Never
place their contents in the flow or repository.

The certificate policy should allow only:

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": "iot:Connect",
      "Resource": "arn:aws:iot:eu-north-1:ACCOUNT_ID:client/mot-node-red-xrpioneer"
    },
    {
      "Effect": "Allow",
      "Action": ["iot:Publish", "iot:RetainPublish"],
      "Resource": "arn:aws:iot:eu-north-1:ACCOUNT_ID:topic/mot/xrpioneer/*"
    }
  ]
}
```

Do not add AWS subscribe, receive or command permissions to this forwarder.

The certificate was validated with an allowed retained publish to
`mot/xrpioneer/system/device_name`; the value reached the vehicle-state table. A
QoS 1 publish to `mot/not-allowed/...` was rejected by AWS IoT with connection
loss, confirming the namespace restriction.

## Portal and history

AWS will build current state as soon as the first live message arrives. Portal
visibility additionally requires an ACTIVE `UserVehicleAccess` assignment for
`xrpioneer`. History requires adding `xrpioneer` to the separate History vehicle
allowlist; neither access is granted merely by publishing telemetry.

On 2026-08-04 the controlled Cognito user
`xruser@microlino-open-telemetry.ch` was invited and assigned exclusively to
`xrpioneer` as ACTIVE OWNER. The user remains in Cognito
`FORCE_CHANGE_PASSWORD` state until the invitation is accepted. History remains
enabled for this identity, using 5-minute core buckets and one-minute
active-driving Speed samples with the shared 31-day TTL. No backfill occurs;
collection starts with live forwarded state events.

The deployed direct-state path was validated with current state, payload types,
successive update frequency and absence of forwarding loops before retaining the
bounded History enablement.
