# Portal Production Deployment

## Scope

The production portal is hosted at:

- portal: `https://www.microlino-open-telemetry.ch/dashboard/`
- OAuth callback and post-logout destination:
  `https://www.microlino-open-telemetry.ch/dashboard/`

The project landing page at `/` and the validated fallback portal at `/motbeta/`
are not part of this operation and must remain unchanged.

The Cognito app client already permits `/dashboard/` as callback and logout URL.
The portal processes the authorization response on that page, so no separate
`/dashboard/callback/` registration or AWS change is required for this promotion.

## Upload Package

Build the ignored upload directory from the tracked portal sources and replace
the generic configuration with the reviewed production configuration:

```console
mkdir -p build/portal/dashboard-rel-001-pilot.1
rsync -a --delete --delete-excluded \
  --exclude '.DS_Store' \
  --exclude 'config.js' \
  --exclude 'config.example.js' \
  --exclude 'config.*.example.js' \
  dashboard/ build/portal/dashboard-rel-001-pilot.1/
cp dashboard/config.production.example.js \
  build/portal/dashboard-rel-001-pilot.1/config.js
```

`build/portal/` is intentionally ignored. The generated `config.js` contains
public browser settings, not device certificates, passwords or private keys.

## Backup and Upload with FileZilla

1. Connect using FTP over TLS.
2. Download the complete existing server directory `/dashboard/` into a dated
   local backup outside the repository.
3. Confirm that the backup contains the expected files before continuing.
4. Upload the **contents** of
   `build/portal/dashboard-rel-001-pilot.1/` into `/dashboard/`. Do not upload
   the enclosing `dashboard-rel-001-pilot.1` directory.
5. Do not modify `/`, `/motbeta/` or their redirects.

If the host supports safe remote renaming, renaming the old `/dashboard/`
directory to a dated backup and creating a fresh `/dashboard/` reduces the risk
of stale files. The downloaded backup remains the authoritative rollback copy.

## Acceptance Test

Use a private browser window and verify:

1. `/` still shows the project landing page.
2. `/dashboard/` loads the portal without JavaScript or HTTP errors.
3. Login returns to `/dashboard/` and the assigned vehicles are visible.
4. A normal user sees only their own vehicles.
5. An administrator can use the reviewed onboarding functions.
6. Live, cloud and stale/offline states behave as in the `/motbeta/` acceptance.
7. Logout returns to `/dashboard/` without a Cognito error page.
8. `/motbeta/` still loads and remains available as fallback.

Because the hosting provider has not yet confirmed OAuth-query redaction, do not
copy authorization query values from access logs into tickets or project files.
The bounded acceptance in the cloud risk register continues to apply.

## Rollback

If a release-blocking failure occurs, restore the complete downloaded
`/dashboard/` backup through FileZilla and repeat the basic load, login and logout
checks. Do not redirect users to `/motbeta/` automatically; keep that path as the
explicitly controlled fallback until rollback has been verified.

## References

- [Beta deployment](portal-beta-deployment.md)
- [REL-001 release-candidate validation](../testing/REL-001-rc-validation.md)
- [Cloud risk register](../security/cloud-risk-register.md)
