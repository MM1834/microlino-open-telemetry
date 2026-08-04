# Node-RED Legacy MQTT to AWS IoT Forwarder

> **Status:** AWS identity validated; Node-RED deployment pending
>
> **Source identity:** ioBroker `mqtt.3` / MQTT `mot/xrpioneer/#`
>
> **Target identity:** AWS IoT `mot/xrpioneer/#`

## Purpose

This flow forwards telemetry from the first-generation ESP32 and its local MQTT
broker into the current AWS pipeline without changing the old firmware. It uses a
dedicated AWS IoT identity and does not expose the unencrypted broker to AWS.

Import [the flow template](../../tools/node-red/mot-xrpioneer-aws-forwarder.json)
through Node-RED's **Import → Clipboard/File** dialog. Keep the flow disabled until
both broker configuration nodes are complete.

## Forwarded topics

Only the supplied `mot/xrpioneer` suffixes are allowed. Unknown topics are blocked
and reported to the debug sidebar. Boolean `0/1` payloads for
`charging/is_charging` and `charging/plugged` become JSON booleans. Numeric topics
become JSON numbers; bounded system values remain strings.

Retained messages replayed when Node-RED connects to the local broker are dropped.
This prevents an old retained snapshot from appearing as fresh AWS history. The
next live publish is forwarded and retained in AWS.

## Local broker node

Open **ioBroker mqtt.3 · CONFIGURE** and set:

- host and port of the local MQTT broker;
- username/password if used;
- no TLS only when the connection remains on the trusted local host/network.

The flow subscribes to `mot/xrpioneer/#`, corresponding to the supplied ioBroker
object IDs `mqtt.3.mot.xrpioneer.*`.

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
enabled for this identity as of 2026-08-04, using 5-minute core and 15-minute
Speed buckets with the shared 31-day TTL. No backfill occurs; collection starts
with the next live forwarded publish.

Validate initially with History disabled for this identity. Confirm current state,
payload types, update frequency and absence of forwarding loops before enabling
bounded history.
