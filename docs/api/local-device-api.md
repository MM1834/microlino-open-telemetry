# Local Device HTTP API

> **Status:** Route registration confirmed in source; responses/runtime unverified
>
> **Audience:** Firmware developer, beta-support author and local tool developer
>
> **Sources:** `web_ui.cpp`, `ota_web.cpp` and `lilygo_web.cpp`

## Trust boundary

These endpoints are hosted by the physical device and currently have no application
authentication. They are intended for a directly connected/local network and must
not be exposed as portal endpoints or published to the Internet.

Configuration export can include secrets. Support procedures must never request an
unredacted export through public issue trackers or chat.

## Common routes

| Method | Route | Purpose |
|---|---|---|
| GET | `/` | Wizard until onboarding complete, otherwise local status/root page |
| GET | `/wizard` | Local device setup wizard |
| GET | `/api/onboarding` | Onboarding/readiness status |
| POST | `/api/onboarding/complete` | Mark local setup complete |
| POST | `/api/onboarding/restart` | Restart local setup state |
| GET/POST | `/api/config` | Export/import configuration |
| GET | `/api/config/export` | Configuration export alias |
| POST | `/api/config/import` | Configuration import alias |
| POST | `/config/import` | Compatibility import route |
| GET | `/api/readiness` | Shared readiness model |
| GET | `/api/status` | Board status JSON |
| POST | `/factory-reset` | Clear configuration and restart/reset flow |

The browser form-save paths differ: ESP32-WROOM uses `POST /save`; LilyGO uses
`POST /config/save`.

Some ESP32-WROOM registrations omit an explicit HTTP-method constraint and are
therefore broader than the intended GET usage. The table records intended client
usage, not an authorization guarantee.

## ESP32-WROOM-specific routes

| Method | Route | Purpose |
|---|---|---|
| GET | `/status` | Local HTML status page |
| GET | `/api/mqtt-test` | MQTT diagnostic/test endpoint |
| GET | `/api/system-health` | System health JSON |
| GET | `/api/gps` | Optional GPS status |
| GET | `/api/abrp/status` | ABRP state |
| POST | `/api/abrp/test` | Trigger local ABRP test action |
| GET | `/update`, `/ota` | OTA upload page |
| POST | `/update` | Firmware upload/update |

The OTA module does not register `POST /ota/update` for ESP32-WROOM.

## LilyGO-specific routes

| Method | Route | Purpose |
|---|---|---|
| GET | `/api/telemetry` | Shared normalized telemetry JSON |
| GET | `/api/lilygo/network` | Network status |
| GET | `/api/lilygo/modem` | Modem status |
| GET | `/api/lilygo/gps`, `/api/lilygo/gnss` | GPS aliases |
| GET | `/api/lilygo/can` | CAN status |
| GET | `/api/lilygo/can/frames` | Recent/raw CAN frames |
| GET | `/api/lilygo/mqtt` | MQTT/AWS status |
| GET | `/api/lilygo/mqtt/debug` | MQTT diagnostics |
| GET | `/api/lilygo/abrp` | ABRP state |
| POST | `/api/lilygo/abrp/test` | Trigger local ABRP test action |
| GET | `/api/lilygo/lte/debug` | LTE diagnostics |
| GET | `/api/lilygo/lte/rx-debug` | LTE receive diagnostics |
| GET/POST | `/api/lilygo/lte/tcp-test` | LTE TCP diagnostic action/status |
| GET | `/api/lilygo/lte/mqtt-trace` | MQTT/LTE trace |
| POST | `/api/lilygo/lte/mqtt-trace/clear` | Clear local trace |
| GET | `/ota` | OTA upload page |
| POST | `/ota/update` | Firmware upload/update |

Diagnostic POST endpoints and OTA/factory reset are mutating local operations even
though they do not change cloud state. They require explicit user action and must
not be invoked during read-only support collection.

## Documentation gaps

- Response schemas are not yet normalized for all board-specific endpoints.
- Route parity is incomplete and should not be implied by the shared config API.
- Current access control relies on network/physical locality rather than login.
- Portal onboarding must use a hosted authenticated backend, not these routes.

## Related documents

- [Configuration API](configuration-api.md)
- [Firmware overview](../firmware/overview.md)
- [Local WebUI](../webui/overview.md)
