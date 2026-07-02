# Sprint 3.5 — Firmware Refactor Foundation

This sprint introduces a clean firmware structure for Microlino Open Telemetry.

Goal:

- keep platform-specific code in `firmware/esp32-wroom/`
- keep reusable firmware modules in `firmware/common/`
- prepare the codebase for LilyGO, LTE/GPS and dual-CAN
- keep one telemetry model as the central data structure

This package intentionally focuses on structure and shared interfaces. It does not replace the tested WROOM firmware logic yet.

Recommended workflow:

```bash
git checkout develop
unzip mot-sprint3.5-firmware-refactor.zip
cp -R mot-sprint3.5-firmware-refactor/. .
rm -rf mot-sprint3.5-firmware-refactor
git add .
git commit -m "Add Sprint 3.5 firmware refactor foundation"
git push
```

After this sprint, we migrate the working WROOM firmware module by module.
