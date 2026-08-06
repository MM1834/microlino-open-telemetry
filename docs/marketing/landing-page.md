# Public Landing Page

> **Status:** Deployed and maintainer-validated
>
> **Owner:** WEB-001

The repository package was deployed to the public website and successfully tested
by the maintainer on desktop and smartphone on 2026-08-05.

The 2026-08-06 repository revision adds the bounded N16 dual-CAN pilot result,
new battery energy-flow fields and measured Pioneer road/charging evidence. Its
portal image contains real project-vehicle telemetry. This maintenance revision
is not considered hosted until the maintainer uploads and smoke-tests it.

The public project landing page is maintained under `build/landing/current/`.
It is a static, dependency-free website intended for the root of
`https://www.microlino-open-telemetry.ch/`. The authenticated portal remains a
separate application at `/dashboard/`.

## Content boundary

The landing page presents:

- the passive, local-first telemetry architecture;
- the distinct MOT cloud, local WebUI and direct ABRP service paths;
- current portal capabilities;
- the controlled beta onboarding journey;
- a dated project-status summary derived from governance.

It does not contain Cognito configuration, API endpoints, credentials or runtime
telemetry. The dashboard call-to-action links to `/dashboard/`.

## Local preview

From the repository root:

```sh
python3 -m http.server 8080 --directory build/landing/current
```

Then open `http://localhost:8080/`.

## Publication boundary

Deployment to `/` is a manual operator action performed by the maintainer with
FileZilla over FTPS. The repository does not automate or store host credentials.
Before upload:

1. back up the current hosted root;
2. validate the exact repository revision locally;
3. confirm `/dashboard/` and `/motbeta/` are not included in the replacement set;
4. use FileZilla to upload only the contents of `build/landing/current/` to the
   hosted root;
5. smoke-test `/`, `/dashboard/` and `/motbeta/`;
6. record the deployed commit and rollback backup.

## Status maintenance

Update public status claims only after checking
`docs/governance/CURRENT_STATUS.md` and `docs/governance/WORK_ORDER.md`.
