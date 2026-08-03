# DOK-003.3 firmware documentation migration

This package replaces the short v2 firmware-guide stubs with consolidated permanent documentation and preserves selected historic documents under `docs/developer/`.

## Main firmware guide

Files in `docs/firmware/` are intended to be the permanent, current documentation.

## Developer archive

29 selected historical documents were copied into:

```text
docs/developer/lte/
docs/developer/mqtt/
docs/developer/can/
docs/developer/operations/
docs/developer/release-notes/
```

Each copied document has a historical-status banner.

## Cleanup not performed automatically

This package does not delete the original old documents from `docs/firmware/`. After review, remove only the originals that now have an equivalent permanent chapter or a preserved developer copy.

Use `git status` and a link check before deleting old files.
