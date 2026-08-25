# Firmware configuration

Configuration is stored in ESP32 Preferences/NVS and can be edited through the local WebUI.

Decoder assignment is a logical runtime setting, not a decoder restriction tied
to one controller. Multi-CAN targets store both `can1` and `can2`; single-CAN
targets retain Display-CAN as the default for their available input.

## Main settings

| Setting | Purpose |
|---|---|
| `deviceName` | Stable device identity and MQTT client-name source |
| `vehicleId` | Vehicle component in MQTT topic paths |
| `mqttPrefix` | Root MQTT topic prefix |
| `wifiSsid` / `wifiPass` | Preferred home-WiFi station credentials |
| `wifi2Ssid` / `wifi2Pass` | C6 second/mobile-hotspot credentials |
| `lteApn` / user / password | Mobile packet-data configuration |
| `mqttHost` / `mqttPort` | MQTT broker endpoint |
| `mqttUser` / `mqttPass` | MQTT credentials |
| `otaEnabled` / `otaPassword` | OTA configuration |
| `abrpEnabled` | Enables optional ABRP integration |
| `offlineCacheEnabled` | C6-only, default-off SOC/active-Speed outage cache |
| ABRP API key/token | ABRP credentials |
| `can1Profile` / `can2Profile` | Independent decoder assignment for each physical CAN input |

## Stable identity

The device name should remain stable after deployment. It is used in status information and may be used to derive the MQTT client ID.

Use a unique value for every physical unit.

## Optional services

MQTT should only be started when a broker host is configured. ABRP should only be started when it is enabled and its required credentials are present.

## Configuration workflow

1. Configure the device through the WebUI.
2. Verify WiFi or LTE status.
3. Verify MQTT over WiFi first.
4. Export a JSON backup.
5. Store the backup securely.

On C6, credentials can be configured through the authenticated WebUI or the
115200-baud serial console. `wifi set <ssid>|<password>` configures preferred Home
WiFi; `wifi2 set <ssid>|<password>` configures the second/mobile hotspot.
`wifi clear` and `wifi2 clear` remove the respective profile. `wifi status`
reports selection state without secret echo. A restart applies configuration
changes made through the console.

The canonical C6 builds load the same four per-device credential files from
`/aws` in LittleFS as WROOM and LilyGO. `aws status` reports credential and
connection state without printing key material.

C6 ABRP is independently enabled at runtime and may operate alongside AWS IoT.
The API key and user token are stored in NVS, accepted only through authenticated
same-origin configuration routes and omitted from normal backup exports and
diagnostics. The local onboarding wizard records only its progress and completion
state; it is not portal/account onboarding.

On C6 the wizard also stores its current step so required reboots resume the local
setup journey. Its WiFi, CAN and service forms use the same persisted configuration
model as the later administration page. The protected AP stays available until
explicit wizard completion; afterward the normal station-stability and fallback
rules apply. This local state remains separate from portal/account onboarding.

Both C6 targets expose an authenticated `Offline History cache` option. It is
off by default and records only SOC plus active one-minute Speed/terminal zero
while AWS IoT is disconnected and trustworthy UTC has previously been obtained.
It never stores GPS/location. XIAO is capped at 128 KiB and N16 at 256 KiB;
reaching the cap stops new writes instead of rotating flash. Disabling the option
or factory reset purges queued records. Configuration export includes only the
boolean setting, never cached telemetry.

GPS hardware is recommended, but not required, for cache deployments. The C6 GPS
service already validates GNSS date/time and sets the shared system UTC clock;
the cache reads that clock exactly as it reads NTP-derived time. This allows a
GPS-equipped adapter to become timestamp-capable without WiFi in principle, while
no-GPS hardware relies on NTP and uninterrupted power. Physical offline-cold-start
acceptance and stale-time hardening remain open. The cache never writes GPS
coordinates or routes into its journal or Backfill envelope.
