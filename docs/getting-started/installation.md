# Installation

## Build firmware

```bash
cd firmware/lilygo-t-a7670
pio run
```

## Upload firmware

```bash
pio run -t upload
```

## Upload filesystem

```bash
pio run -t uploadfs
```

If `uploadfs` reports `can't read source directory`, skip it unless the environment contains WebUI assets in `data/`.

## Restore configuration

Use the WebUI Backup/Restore page.

> TODO screenshot: `docs/images/screenshots/backup-restore.png`
