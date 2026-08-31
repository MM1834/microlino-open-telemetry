# Enclosure and installation

The final hardware should be installed in an enclosure that protects the electronics while keeping USB, antennas and wiring serviceable.

![LilyGO case open](../assets/images/hardware/lilygo-t-a7670-case-open.png)

![LilyGO case with GPS](../assets/images/hardware/lilygo-t-a7670-case-with-gps.png)

![Installed ESP32 system](../assets/images/hardware/system-esp32-installed.png)

## Recommendations

- Provide strain relief for OBD/CAN wiring.
- Keep USB accessible for recovery flashing.
- Keep LTE and GPS antennas away from shielding.
- Mount a ceramic GNSS patch horizontally with its free patch/ceramic face toward
  the sky and its ground-plane/PCB/cable or intended adhesive face toward the
  mounting surface. Do not infer the active face from colour alone; confirm every
  sourced antenna variant from its drawing or a controlled outdoor A/B test.
- Keep metal, metallized coatings, batteries, PCBs and cable bundles out of the
  antenna's sky-facing hemisphere. A thin non-metallized plastic enclosure wall
  above the patch is preferred.
- Allow ventilation around the LTE modem.
- Label board type and firmware variant.
