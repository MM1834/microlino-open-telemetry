# Release process

> **Status:** Current release workflow

## Branch and environment model

- Feature branches contain reviewed increments.
- `develop` is the integration branch for release candidates.
- `main` contains accepted repository releases.
- There is no `deploy` branch. A deployed environment must be recorded against an
  exact commit; deployment state is not inferred from a branch name.

Portal paths such as `/MOTbeta/` identify hosted environments, not Git branches.
Pilot users are operationally supported early users, not a separate account type.

## Feature integration

1. Commit and push the feature branch.
2. Run the full tests and document external validation against the exact commit.
3. Open a pull request from the feature branch to `develop`.
4. Review scope, secrets/ignored artifacts, release notes and deferred behaviour.
5. Merge only after the active sprint gates pass.

Prefer the repository host's pull-request merge controls. Do not merge an active
feature branch directly into `main`.

## Release candidate

After integration into `develop`:

1. verify the exact deployed commit and effective cloud configuration;
2. run the controlled pilot and rollback checks;
3. create release notes listing included, deferred and known-risk behaviour;
4. use a prerelease tag only when the maintainer has approved its version and
   evidence.

## Main release

1. Fetch the current remote branches and confirm that `develop` contains every
   intended change from `main`.
2. Resolve divergent history through a reviewed integration branch or pull
   request; do not discard either side.
3. Open and review a pull request from the accepted `develop` state to `main`.
4. Re-run release gates on the exact merge candidate.
5. Merge, create the approved annotated release tag, and push the tag.
6. Record deployment commit, configuration, smoke-test evidence and rollback
   point.

Firmware upload, device credential mutation, cloud deployment and public portal
promotion remain separately approved operations. A Git merge alone authorizes
none of them.

## Current portal release

The active release definition is
[REL-001 — Portal Pilot Release Readiness](../project/sprints/REL-001.md). Its
version is selected only after the fresh-device pilot gate and `develop`
integration; no version number is implied by this document.
