# Consolidated Repository History

> **Status:** Historical recovery record
>
> **Consolidated:** 2026-08-04 for `v1.0.0-rc.1`

The current working tree intentionally omits superseded sprint packages, manifests,
patches, intermediate release notes, hotfix copies and historical README files.
Current engineering guidance lives in the canonical documentation linked from
[`docs/README.md`](../README.md).

The removed pre-RC material is preserved byte-for-byte in:

- `v1.0.0-rc.1-preconsolidation.tar.gz`
- SHA-256: `bd9934d882a35a5eb88d4b6aae62efbee7f59fb4e116a3882f232a6f88d6dde8`
- archive entries: 288

Verify and inspect without extracting:

```sh
shasum -a 256 docs/history/v1.0.0-rc.1-preconsolidation.tar.gz
tar -tzf docs/history/v1.0.0-rc.1-preconsolidation.tar.gz
```

Prefer Git history for line-level investigation. Extract the package only into a
temporary directory; its documents describe older revisions and are not current
deployment or security instructions.
