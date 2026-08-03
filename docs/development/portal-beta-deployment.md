# Beta portal deployment

Status: AWS configuration deployed; portal upload pending

The controlled beta portal is hosted separately from the existing dashboard:

- portal: `https://www.microlino-open-telemetry.ch/motbeta/`
- OAuth callback: `https://www.microlino-open-telemetry.ch/motbeta/callback/`
- post-logout destination: `https://www.microlino-open-telemetry.ch/motbeta/`
- exact API CORS origin: `https://www.microlino-open-telemetry.ch`

The origin contains only scheme and host. URL paths such as `/motbeta/` do not
belong in a CORS origin.

## Deployment boundary

The repository does not contain web-server or FTP credentials. Upload is a
manual operator action using FileZilla with FTP over TLS. The existing
`/dashboard/` directory remains in place during beta validation.

## Prepare the local upload directory

1. Make a local copy of the complete `dashboard/` directory.
2. Name the copied directory `motbeta`.
3. In that copy, replace `config.js` with the contents of
   `dashboard/config.beta.example.js`.
4. Do not upload local backups, logs, credentials, `.env` files, or files from
   `secrets/`.
5. Upload the contents to the web-server directory that maps to `/motbeta/`.

The Cognito identifiers and API endpoints in the browser configuration are
public application coordinates, not device or AWS credentials.

## Required AWS configuration before login testing

The upload alone is insufficient. Before the hosted login can work, a reviewed
CloudFormation change must:

1. add the exact beta callback and logout URLs to the existing Cognito app
   client without removing the currently registered URLs;
2. set the foundation API CORS origin to the exact HTTPS origin above;
3. set the onboarding API CORS origin to the same exact HTTPS origin.

No wildcard origin is permitted. These changes are deployed only after the
operator reviews the change sets.

### Confirmed deployment, 2026-08-03

The reviewed change set `motbeta-hosted-portal-20260803` was executed on both
stacks:

- `mot-aws-3-1`: `UPDATE_COMPLETE`
- `mot-dev-onboarding`: `UPDATE_COMPLETE`

The effective Cognito app client contains the beta callback and logout URLs in
addition to all previously registered URLs. Both HTTP APIs return the exact
portal origin for matching CORS preflights and no CORS origin header for
`http://localhost:8080` or an unrelated origin. Localhost browser API testing
therefore remains intentionally unavailable while this single-origin beta
configuration is active.

## Hosted smoke test

After upload and AWS configuration:

1. Open `/motbeta/` in a private browser window.
2. Verify login and password flow with a beta account.
3. Verify that the user sees only assigned vehicles.
4. Verify admin claim controls are visible only to `mot-beta-admins`.
5. Verify logout returns to `/motbeta/` without a Cognito error page.
6. Use browser Back after logout and verify that vehicle data is not restored.
7. Verify onboarding claim issue and consumption with a controlled test
   assignment.

Production-host access logs must not retain OAuth `code`, `state`, or claim
proof values. Verify query-string redaction and retention with the hosting
provider before treating the beta portal as operational.

### Confirmed hosting log exposure, 2026-08-03

After controlled hosted login tests, the maintainer confirmed that both
`access_ssl_log` and `proxy_access_ssl_log` contain the `/motbeta/callback/`
query names `code` and `state`. No values were copied into project records.
Successfully exchanged authorization codes are single use and PKCE-bound, but
their presence in two access logs confirms CLOUD-017 rather than resolving it.

The available hosting UI exposes log rotation but no callback-path exclusion or
query-string redaction. Before external pilot accounts are admitted, obtain a
provider-side logging change or record an explicit short-retention exception with
restricted log access and a deletion schedule. Claim proofs remain prohibited in
URLs and logs regardless of any OAuth exception.

### Confirmed end-user smoke test, 2026-08-03

The hosted portal was tested through the canonical `www` URL with
`info@muehlberg.ch`:

- portal loading and Cognito login passed;
- only the assigned vehicle `beta-01` was visible;
- vehicle data loaded as expected;
- logout returned to `/motbeta/` without a Cognito error page;
- the signed-out portal did not expose the previous authenticated state.

The non-`www` host currently serves the same files without redirecting to the
canonical `www` host. Until a server-side redirect is configured, all portal
links and tests must use `https://www.microlino-open-telemetry.ch/motbeta/`.

### Confirmed admin smoke test, 2026-08-03

The hosted portal was tested with the beta administrator
`news@muehlberg.ch`:

- Cognito login passed;
- only the assigned vehicle `pioneer` was visible;
- the admin claim controls were visible;
- attempting to issue a claim for the actively assigned `beta-01` returned
  `Fahrzeug ist bereits zugewiesen oder nicht verfügbar`;
- no claim was issued and the existing assignment remained protected;
- logout returned to `/motbeta/` without an error page.

This confirms the hosted role separation, vehicle isolation, onboarding API
connectivity, and active-assignment conflict guard.

### Lowercase path migration, 2026-08-03

The pilot path was migrated from `/MOTbeta/` to the hosting-compatible lowercase
path `/motbeta/`. Change set `motbeta-lowercase-path-20260803` added the lowercase
callback and logout URLs without replacing the Cognito app client. Hosted loading,
login, logout and the non-`www` browser check passed, and the old uppercase server
directory was removed after validation.

An independent HTTP check still receives `200` directly from
`https://microlino-open-telemetry.ch/motbeta/` rather than a server-side `301` to
the canonical `www` host. Safari may visually hide `www` and may reuse cached/HSTS
navigation, so its address-bar presentation is not redirect evidence.

The hosting subscription exposes neither Plesk's preferred-domain selector nor
additional nginx directives. Hosttech Domaincenter contains explicit direct-301
rules, but they do not precede the active Plesk HTTPS virtual host in an independent
client test. The maintainer accepted this as a bounded pilot exception on
2026-08-03: both names serve TLS, while all published links, Cognito callbacks and
API CORS use `www`. Revisit canonical host enforcement before general public
promotion or when the hosting plan changes.
