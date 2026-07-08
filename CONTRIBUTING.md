# Contributing

## Development workflow

1. Create a feature branch from `develop`.
2. Keep hardware-specific changes isolated.
3. Build before committing.
4. Update documentation for user-visible changes.
5. Merge back into `develop`.

## Build check

```bash
cd firmware/lilygo-t-a7670
pio run -t clean
pio run
```

## Commit examples

```text
feat(lilygo): add LTE MQTT transport
fix(config): include ABRP fields in backup JSON
docs(hardware): add WeAct CAN485 board
release: stabilize LilyGO LTE MQTT integration
```
