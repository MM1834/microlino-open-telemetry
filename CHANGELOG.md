
## Unreleased


### SPR-0004B.1.3

- Added the Cognito managed-login domain as an isolated CloudFormation resource.
- Added outputs for the OAuth base, authorization, token, and logout endpoints.
- Added developer, administrator, and end-user domain documentation.



### SPR-0004B.1.2

- Added a public Cognito dashboard app client without a client secret.
- Enabled authorization-code flow for later PKCE-based browser login.
- Added configurable callback and logout URLs.
- Added token lifetime, revocation, and user-enumeration protections.
- Added developer, administrator, and end-user documentation.

### SPR-0004B.1.1

- Split Cognito delivery into independently deployable increments.
- Added only the Cognito dashboard user pool in the first increment.
- Quoted `MfaConfiguration: "OFF"` to prevent YAML from converting it to Boolean `false`.
- Documented the rollback cause and safe change-set workflow.

- Integrate shared GPS parsing into ESP32-WROOM production firmware.
- Publish location to MQTT/AWS and ABRP only for a current valid fix.
- Preserve retained coordinates and label stale coordinates as the last known location in the dashboard.
# Changelog

## v1.1.0-lilygo-stability

### Added
- LilyGO T-A7670G firmware path.
- LewisXhe TinyGSM A76XXSSL transport for A7670G-LLSE.
- LTE modem initialization and GPRS/PDP connection.
- MQTT over LTE.
- External L76K GPS support.
- CAN receive diagnostics.
- Backup/Restore for JSON configuration.
- Factory Reset.
- Modem recovery after OTA/reset.

### Known issues
- ABRP over LTE HTTPS is disabled/deferred.
- Device online heartbeat is planned but not yet implemented.

## SPR-0004B.1 - Cognito infrastructure

- Added an email-based Amazon Cognito user pool and public dashboard app client.
- Disabled public self-registration and enabled deletion protection by default.
- Added developer, administrator and end-user authentication documentation.
