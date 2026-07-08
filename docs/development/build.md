# Build

## LilyGO

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
pio run -t upload
```

If no `data/` directory exists, `uploadfs` is not needed.
