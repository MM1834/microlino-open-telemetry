# USB-DIAG-001 — Freigegebene lokale Adapterdiagnose

**Status:** Complete — Web Serial reset correction physically accepted

**Prepared:** 2026-09-17

## Objective

Allow support to grant one authenticated portal user a bounded, read-only USB
diagnostic session when an adapter appears connected but does not deliver the
expected AWS telemetry.

## Product and security contract

- Use a separate `usb-diagnostics` grant for 1–168 hours; WebFlash and password
  recovery grants do not authorize diagnostics.
- Require the existing administrator group for grant, revoke and active-grant
  inventory operations.
- Open the normally running adapter through Web Serial at 115200 baud; never
  enter the bootloader and never write configuration.
- Execute only the existing read-only console commands `profiles`, `wifi status`,
  `aws status` and `cache status`. New firmware additionally accepts the read-only
  `support status` command; the original commands remain in the request for older
  adapters.
- Display only complete lines beginning with the explicit safe prefixes
  `Profiles:`, `Profiles configured:`, `WiFi:`, `AWS IoT:` or `Offline cache:`.
  All startup and unrelated console output is discarded.
- Keep diagnostic content local to the browser. AWS audit records contain only
  grant/revoke, authorization and success/failure outcomes with an operation ID.
- Provide a local copy action for the filtered support output.
- Degrade safely when Web Serial is unavailable. Safari and Firefox do not
  currently provide this interface; Chrome/Edge-class browsers are required.

## Repository implementation

- Five JWT-protected API routes mirror the bounded password-recovery lifecycle.
- The existing encrypted TTL grant table stores the independent action without
  adding a table or exposing a firmware artifact.
- Administration can grant, revoke and list active USB-diagnostic permissions.
- The main portal exposes the diagnostic card only to the exact authorized user.
- German, English, French and Italian labels are included.

## Validation

- Onboarding handler and route-contract suites cover exact grant separation,
  expiry/action checks and the absence of diagnostic output in audit records.
- Portal contracts cover the separate administrator grant, protected API paths,
  fixed commands, allowlisted prefixes and local-only result reporting.
- Python and JavaScript syntax checks and whitespace validation pass.

## Deployment boundary

The new API routes are a CloudFormation infrastructure change. They must not be
deployed without a reviewed no-replacement Change Set and explicit maintainer
approval. Hosted USB acceptance follows that deployment.

## Deployment record

Change Set `usb-diag-001-20260917` was reviewed and executed on
`mot-dev-onboarding` on 2026-09-17. It added exactly the five JWT-authorized USB
diagnostic routes and updated the existing Lambda plus its derived integration;
no resource was deleted or replaced. All existing firmware, Cognito, table and
origin parameters remained unchanged. The stack returned to `UPDATE_COMPLETE`,
the Lambda reports `Active`/`Successful`, all five routes reference the existing
JWT authorizer, anonymous access returns HTTP 401 and a direct read-only smoke
test for an ungranted subject returns `authorized:false` without function error.

The portal was uploaded and productive acceptance passed with two different
users on 2026-09-17. Administrator grant, user-specific visibility, physical USB
read-out, filtered technical details, local copy action and subsequent access
handling all behaved as designed. USB-DIAG-001 is complete.

## Local WiFi identification extension

Support follow-up after acceptance showed that a loose connection can leave an
adapter in an ambiguous network state. The diagnosis therefore also accepts the
local-only `WiFi profiles:` line. A firmware follow-up prints the configured home
and mobile SSID plus the already available active-profile IP address, but never
prints either password. Control characters in SSIDs are replaced before serial
output so one name cannot create a second apparent diagnostic line. Existing
firmware continues to provide IP and configured/not-configured flags; SSID names
appear after installation of a firmware version containing this extension.

## Backward-compatible support status

The firmware follow-up adds one compact `support status` command without removing
or changing any existing console command. It reports firmware, device and board,
uptime and reset reason, heap watermarks, detailed WiFi connection and disconnect
state, AWS/TLS status, offline-cache counters, clock validity and CAN1/CAN2 frame
health. This makes power interruptions, weak or switching WiFi, invalid time,
cloud handshake failures and a missing vehicle bus distinguishable during one
authorized session. Passwords, certificates, private keys and tokens remain
excluded.

The portal sends `support status` first and then still sends `profiles`,
`wifi status`, `aws status` and `cache status`. Older firmware ignores the unknown
new command and answers the established commands; updated firmware supplies both
the richer support report and the established fallback lines.

## Web Serial reset follow-up

A later macOS Chrome/Edge retest reproduced an immediate ESP32-C6 reset when the
portal opened USB diagnosis. The captured serial log contains a new ROM boot with
`rst:0x1 (POWERON)` but no panic, watchdog or brownout. PlatformIO Device Monitor,
direct `support status` input and the Spacehuhn Web Serial terminal remain stable
on the same adapters. Code comparison showed that the stable terminal opens the
port without changing DTR/RTS, while the portal additionally requested a
`false`/`false` signal state after opening. Because Web Serial applies the two
control signals independently, that request can transiently activate the C6
auto-reset circuit. The portal correction therefore leaves DTR/RTS untouched in
the read-only diagnostic path. Physical no-reset acceptance on the affected
Mac/CH343 combinations was required before this sprint could return to complete.

Physical retesting on 2026-09-17 passed: opening the authorized diagnosis left
the adapter stable and returned the requested filtered support data. This closes
the reset follow-up and returns USB-DIAG-001 to complete.
